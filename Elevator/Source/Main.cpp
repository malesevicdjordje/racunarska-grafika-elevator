#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include "../Header/Util.h"

// --------------------- GLOBAL CONSTANTS -----------------

// PROZOR
constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 800;

// SPRATOVI
constexpr int FLOOR_COUNT = 8;
constexpr float FLOOR_HEIGHT = 2.0f / FLOOR_COUNT;
constexpr float FLOOR_LINE_X = 0.0f;
constexpr float FLOOR_LINE_WIDTH = 2.0f;
constexpr float FLOOR_LINE_HEIGHT = 0.005f;

// LIFT
constexpr float ELEVATOR_X = 0.7f;
constexpr float ELEVATOR_Y = -1.0f + (2 * FLOOR_HEIGHT);
constexpr float ELEVATOR_WIDTH = 0.2f;
constexpr float ELEVATOR_HEIGHT = 0.15f;
constexpr float ELEVATOR_SPEED = 0.4f;

// VRATA
constexpr float DOOR_MAX_OFFSET = 0.15f;
constexpr float DOOR_OPEN_TIME = 5.0f;
constexpr float DOOR_ANIM_SPEED = 0.1f;

// OSOBA
constexpr float PERSON_X = 0.0f;
constexpr float PERSON_Y = -1.0f + (1 * FLOOR_HEIGHT);
constexpr float PERSON2_Y = -1.0f + (3 * FLOOR_HEIGHT);
constexpr float PERSON_WIDTH = 0.10f;
constexpr float PERSON_HEIGHT = 0.12f;
constexpr int   PERSON_TEXTURE_ID = 1;
constexpr int   PERSON2_TEXTURE_ID = 2;
constexpr float PERSON_SPEED = 0.8f;
constexpr float PERSON_ENTER_LERP = 3.0f;
constexpr float PERSON_CENTER_LERP = 2.0f;
constexpr float PERSON_ENTER_THRESHOLD = 0.25f;

// KRETANJE / TOLERANCIJE
constexpr float POSITION_EPSILON = 0.01f;
constexpr float FLOOR_MATCH_EPSILON = FLOOR_HEIGHT * 0.1f;

// HODNIK
constexpr float HALLWAY_LEFT_X = 0.0f;
constexpr float HALLWAY_RIGHT_X = 0.68f;

// DUGMAD
constexpr float BUTTON_WIDTH = 0.10f;
constexpr float BUTTON_HEIGHT = 0.08f;
constexpr float BUTTON_SPACING = 0.11f;
constexpr float BUTTON_START_X = -0.92f;
constexpr float BUTTON_START_Y = -0.9f;

// KONTROLE
constexpr float CONTROL_WIDTH = 0.14f;
constexpr float CONTROL_HEIGHT = 0.10f;

// WATERMARK
constexpr float WATERMARK_X = -0.95f;
constexpr float WATERMARK_Y =  0.85f;
constexpr float WATERMARK_W =  0.4f;
constexpr float WATERMARK_H =  0.1f;

// --------------------- Strukture ------------------------
struct Rectangle {
    float x, y, w, h;
    float r, g, b;
    unsigned int textureID;
    bool highlight = false;
    bool flippedPerson = false;
    Rectangle(float x_, float y_, float w_, float h_, float r_ = 1.0f, float g_ = 1.0f, float b_ = 1.0f, unsigned int tex = 0)
        : x(x_), y(y_), w(w_), h(h_), r(r_), g(g_), b(b_), textureID(tex) {
    }
};


struct Scene {
    Rectangle person{ PERSON_X, PERSON_Y, PERSON_WIDTH, PERSON_HEIGHT, 1, 0, 0, PERSON_TEXTURE_ID };
    Rectangle elevator{ ELEVATOR_X, ELEVATOR_Y, ELEVATOR_WIDTH, ELEVATOR_HEIGHT, 0.7f,0.7f,0.7f };
    Rectangle person2{ PERSON_X, PERSON2_Y, PERSON_WIDTH, PERSON_HEIGHT, 0, 0, 1, PERSON2_TEXTURE_ID };

    bool personInElevator = false;   
    bool personExiting = false;
    bool doorsOpen = false;             
    bool extraDoorOpen = false;         
    bool ventilatorOn = false;          // stanje ventilacije
    bool stopPressed = false;           // stanje lifta (radi/ne radi)

    float elevatorSpeed = ELEVATOR_SPEED;       // brzina lifta prilikom pomeranja
    float personSpeed = PERSON_SPEED;           // brzina osobe prilikom pomeranja
    float floorHeight = FLOOR_HEIGHT;           // visina svakog sprata
    float doorDuration = DOOR_OPEN_TIME;        // trajanje otvorenih vrata
    float doorOffset = 0.0f;                    // razmak po sirini left i right vrata od elevatora 
    float doorTimer = 0.0f;                     // tajmer otvorenih vrata
    bool stopRequested = false;
    int stopTargetFloor = -1;

    std::vector<Rectangle> buttons;     // dugmici za spratove lifta
    std::vector<Rectangle> floors;      // linije spratova
    std::vector<bool> floorSelected;    // vektor selektovanih spratova
    std::vector<int> targetFloors;      // vektor ciljanih spratova
    std::vector<int> externalCalls;     // red poziva spolja
    bool person2InElevator = false;
    bool person2Entering = false;


    // Animacija ulaska/izlaska osobe
    bool personEntering = false;
    float enterTimer = 0.0f;

    Scene() { 
        floorSelected.resize(8, false); 
        // Generisanje linija spratova
        for (int i = 0;i < 8;i++) {
            float y = -1.0f + (i * floorHeight);
            floors.emplace_back(FLOOR_LINE_X, y, FLOOR_LINE_WIDTH, FLOOR_LINE_HEIGHT, 0.1f, 0.1f, 0.1f);
        }
    }

    float getFloorY(int floor)
    {
        return -1.0f + floor * floorHeight;
    }

    bool isSameFloor(const Rectangle& a, const Rectangle& b) {
        return fabs(a.y - b.y) < floorHeight * 0.1f;
    }

    bool isAnyoneInElevator() const {
        return personInElevator || person2InElevator;
    }
    
    // Funkcija za azuriranje pozicije i kretanja osobe
    void updateGenericPerson(
        Rectangle& p,
        bool& inElevator,
        float dt,
        bool moveLeft,
        bool moveRight
    )
    {
        // ===================
        // OSOBA NIJE U LIFTU
        // ===================
        if (!inElevator)
        {
            if (moveLeft)
                p.x -= personSpeed * dt;
            if (moveRight)
                p.x += personSpeed * dt;

            // granice hodnika
            if (p.x < HALLWAY_LEFT_X) p.x = HALLWAY_LEFT_X;
            if (p.x + p.w > HALLWAY_RIGHT_X) p.x = HALLWAY_RIGHT_X;

            // ulazak u lift
            bool touchingLift = (p.x + p.w >= elevator.x);

            if (touchingLift && moveRight && doorsOpen && isSameFloor(p, elevator))
            {
                float targetX = elevator.x + elevator.w / 2 - p.w / 2;
                p.x += (targetX - p.x) * PERSON_ENTER_LERP * dt;

                if (fabs(p.x - targetX) < PERSON_ENTER_THRESHOLD)
                {
                    p.x = targetX;
                    inElevator = true;
                    std::cout << ">>> Osoba usla u lift\n";
                }
            }
            return;
        }

        // ===================
        // OSOBA JE U LIFTU
        // ===================
        if (inElevator)
        {
            if (moveLeft && doorsOpen)
            {
                p.x -= personSpeed * dt;

                if (p.x + p.w < elevator.x)
                {
                    inElevator = false;
                    std::cout << "<<< Osoba izasla iz lifta\n";
                }
            }
            else
            {
                float targetX = elevator.x + elevator.w / 2 - p.w / 2;
                p.x += (targetX - p.x) * PERSON_CENTER_LERP * dt;
            }
        }
    }

    // Funkcija za pozivanje lifta
    void callElevatorFromPerson(Rectangle& p, bool inElevator, bool keyPressed) {
        if (inElevator) return;

        bool touchingLift = (p.x + p.w >= elevator.x);
        if (!touchingLift || !keyPressed) return;

        int floor = round((p.y + 1.0f) / floorHeight);

        // dodaj samo ako vec ne postoji
        if (std::find(externalCalls.begin(), externalCalls.end(), floor) == externalCalls.end()) {
            externalCalls.push_back(floor);
            std::cout << "Spoljni poziv sa sprata: " << floor - 1 << "\n";
        }
    }

    // Funkcija za azuriranje pozicije i kretanja lifta
    void updateElevator(float dt)
    {
        float prevY = elevator.y;

        // =============== STOP SEKVENCA ==================
        if (stopRequested && !doorsOpen)
        {
            float targetY = -1.0f + stopTargetFloor * floorHeight;

            if (fabs(elevator.y - targetY) < POSITION_EPSILON)
            {
                elevator.y = targetY;
                doorsOpen = true;
                doorTimer = 0.0f;
                stopRequested = false;
            }
            else
            {
                elevator.y += (elevator.y < targetY ? elevatorSpeed * dt : -elevatorSpeed * dt);
            }

            // vozi osobe zajedno sa liftom
            if (personInElevator)
                person.y += (elevator.y - prevY);
            if (person2InElevator)
                person2.y += (elevator.y - prevY);

            return;     // nista drugo, dok traje STOP
        }

        // =============== VRATA OTVORENA ====================
        if (doorsOpen) {
            return; // dok su vrata otvorena, lift stoji
        }

        // =============== ODABIR SLEDECEG CILJA ====================
        int targetFloor = -1;

        if (!targetFloors.empty()) {
            targetFloor = targetFloors.front();
        }
        else if (!externalCalls.empty())
        {
            targetFloor = externalCalls.front();
        }
        else
        {
            return;     // nema kretanja
        }

        float targetY = -1.0f + targetFloor * floorHeight;

        // =============== KRETANJE LIFTA ====================
        if (fabs(elevator.y - targetY) < POSITION_EPSILON)
        {
            elevator.y = targetY;
            
            // ======= POZICIONIRANJE OSOBE NA LINIJU SPRATA ============
            if (personInElevator)
                person.y = targetY;
            if (person2InElevator)
                person2.y = targetY;

            doorsOpen = true;
            doorTimer = 0.0f;

            // uklanjanje cilja
            if (!targetFloors.empty())
            {
                targetFloors.erase(targetFloors.begin());
                floorSelected[targetFloor] = false;
                buttons[targetFloor].highlight = false;
            }
            else if (!externalCalls.empty())
            {
                externalCalls.erase(externalCalls.begin());
            }

            return;
        }

        // pomeraj ka cilju
        elevator.y += (elevator.y < targetY ? elevatorSpeed * dt : -elevatorSpeed * dt);

        // =============== VOZNJA OSOBA ====================
        if (personInElevator)
            person.y += (elevator.y - prevY);
        if (person2InElevator)
            person2.y += (elevator.y - prevY);

    }

    // Azuriraj vrata
    void updateDoors(float dt) {

        if (!doorsOpen) return;

        doorTimer += dt;

        // animacija vrata
        if (doorOffset < DOOR_MAX_OFFSET)
            doorOffset += dt * DOOR_ANIM_SPEED;

        // istek vremena
        if (doorTimer >= DOOR_OPEN_TIME)
        {
            doorsOpen = false;
            extraDoorOpen = false;
            doorOffset = 0.0f;
            doorTimer = 0.0f;
        }
    }

    // Funkcija koja na pritisak dugmeta za sprat oznacava taj sprat
    void pressFloorButton(int index) {
        if (!personInElevator && !person2InElevator) return; // obe osobe su van lifta - izlazak

        if (index < 0 || index >= 8) return;

        if (!floorSelected[index]) {
            buttons[index].highlight = true;   // UPALJENA BELA BOJA
            floorSelected[index] = true;
            targetFloors.push_back(index);
        }
    }

    void startExit() {
        if (personInElevator && doorsOpen) {
            person.x = elevator.x - elevator.w / 2 + person.w / 2;      
            personExiting = true;
            personEntering = false;
            std::cout << "Osoba izlazi iz lifta...\n";
        }
    }

    
    void pressOpenDoor() {
        if (doorsOpen && !extraDoorOpen && (personInElevator || person2InElevator)) { 
            doorTimer = 0.0f;           // reset - novih 5 sekundi
            extraDoorOpen = true;       // izvrsava se samo jednom
            std::cout << "Vrata produzena za +5s\n";
        } 
    }
    void pressCloseDoor() { 
        if (doorsOpen) { 
            doorsOpen = false; 
            doorOffset = 0.0f;
            doorTimer = 0.0f;
            extraDoorOpen = false;
            std::cout << "Vrata zatvorena rucno\n";
        } 
    }
    void pressStop() { 
        if (stopRequested) return; // stop je vec aktivan

        // Ako se lift ne krece - nema smisla pritisnuti stop
        if (targetFloors.empty()) return;

        stopRequested = true;

        // trenutni "sprat" kao realan broj
        float currentFloorF = (elevator.y + 1.0f) / floorHeight;

        // smer kretanja
        int currentTarget = targetFloors.front();
        float targetY = -1.0f + currentTarget * floorHeight;

        bool goingUp = targetY > elevator.y;

        if (goingUp) {
            stopTargetFloor = (int)ceil(currentFloorF);
        }
        else {
            stopTargetFloor = (int)floor(currentFloorF);
        }
        
        // zastita granica
        if (stopTargetFloor < 0) stopTargetFloor = 0;
        if (stopTargetFloor > 7) stopTargetFloor = 7;

        // otkazi sve unutrasnje spratove
        targetFloors.clear();
        std::fill(floorSelected.begin(), floorSelected.end(), false);
        for (auto& b : buttons) b.highlight = false;

        std::cout << "STOP aktivan! Zaustavljanje na spratu: " << stopTargetFloor - 1 << "\n";
    }
    void toggleVentilation() { ventilatorOn = !ventilatorOn; }
};

// -------------------- Renderer ------------------------
struct Renderer {
    unsigned int shaderID;
    unsigned int VAO, VBO;

	GLint colorLocation;
	GLint useTextureLocation;
	GLint highlightLocation;
	GLint flippedPersonLocation;
	GLint textureLocation;

	void init(unsigned int shader) {
		shaderID = shader;
		
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        glBufferData(GL_ARRAY_BUFFER, 16*sizeof(float), nullptr, GL_DYNAMIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

		colorLocation = glGetUniformLocation(shaderID, "color");
		useTextureLocation = glGetUniformLocation(shaderID, "useTexture");
		highlightLocation = glGetUniformLocation(shaderID, "highlight");
		flippedPersonLocation = glGetUniformLocation(shaderID, "flippedPerson");
		textureLocation = glGetUniformLocation(shaderID, "tex");

		glUseProgram(shaderID);
		glUniform1i(textureLocation, 0); // Texture unit 0

        glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}


    void drawRect(const Rectangle& rect) {
        float vertices[16] = {
            rect.x, rect.y,                   0.0f, 0.0f,
            rect.x + rect.w, rect.y,          1.0f, 0.0f,
            rect.x + rect.w, rect.y + rect.h, 1.0f, 1.0f,
            rect.x, rect.y + rect.h,          0.0f, 1.0f
        };

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        glUseProgram(shaderID);
        glUniform3f(colorLocation, rect.r, rect.g, rect.b);

        glUniform1i(useTextureLocation, rect.textureID != 0);
        glUniform1i(highlightLocation, rect.highlight ? 1 : 0);
        glUniform1i(flippedPersonLocation, rect.flippedPerson ? 1 : 0);

        if (rect.textureID != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, rect.textureID);
        }

		glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        glBindTexture(GL_TEXTURE_2D, 0);
    }


    void drawElevatorDoors(Rectangle elevator, float offset) {
        Rectangle left(elevator.x, elevator.y, elevator.w / 2 - offset, elevator.h, 0.8f, 0.8f, 0.8f);
        Rectangle right(elevator.x + elevator.w / 2 + offset, elevator.y, elevator.w / 2 - offset, elevator.h, 0.8f, 0.8f, 0.8f);
        drawRect(left); drawRect(right);
    }

	void cleanup() {
		glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
	}
};

void preprocessTexture(unsigned& texture, const char* filepath)
{
    texture = loadImageToTexture(filepath);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Generisanje mipmapa
    glGenerateMipmap(GL_TEXTURE_2D);

    // Podesavanje strategija za wrap-ovanje
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Podesavanje algoritma za smanjivanje i povecavanje rezolucije
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

}

// -------------------- Main ----------------------------
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Lift Project", primary, NULL);
    
    //GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Lift Project", NULL, NULL);
    if (!window) return endProgram("Prozor nije kreiran");
    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) return endProgram("GLEW nije inicijalizovan");

    GLFWcursor* cursorBlack = loadImageToCursor("../Resources/cursor.png");
    GLFWcursor* cursorColor = loadImageToCursor("../Resources/cursorColor.png");
    glfwSetCursor(window, cursorBlack);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.2f, 0.6f, 0.8f, 1.0f);

    Scene scene;
    Renderer renderer;
    renderer.init(createShader("Shaders/rect.vert", "Shaders/rect.frag"));

    // -------------------- Ucitavanje tekstura --------------------
    unsigned int tex_Person, tex_Person2, tex_SU, tex_PR, tex_Open, tex_Close, tex_Stop, tex_Fan;

    preprocessTexture(tex_Person, "../Resources/person.png");
    preprocessTexture(tex_Person2, "../Resources/person2.png");
    preprocessTexture(tex_SU, "../Resources/button_SU.png");
    preprocessTexture(tex_PR, "../Resources/button_PR.png");
    preprocessTexture(tex_Open, "../Resources/button_Open.png");
    preprocessTexture(tex_Close, "../Resources/button_Close.png");
    preprocessTexture(tex_Stop, "../Resources/button_Stop.png");
    preprocessTexture(tex_Fan, "../Resources/button_Fan.png");

    

    std::vector<unsigned int> floorTextures = {
        tex_SU, tex_PR,
        ([]() { unsigned int t; preprocessTexture(t,"../Resources/button_1.png"); return t; })(),
        ([]() { unsigned int t; preprocessTexture(t,"../Resources/button_2.png"); return t; })(),
        ([]() { unsigned int t; preprocessTexture(t,"../Resources/button_3.png"); return t; })(),
        ([]() { unsigned int t; preprocessTexture(t,"../Resources/button_4.png"); return t; })(),
        ([]() { unsigned int t; preprocessTexture(t,"../Resources/button_5.png"); return t; })(),
        ([]() { unsigned int t; preprocessTexture(t,"../Resources/button_6.png"); return t; })()
    };

    unsigned int tex_Watermark;
    preprocessTexture(tex_Watermark, "../Resources/watermark.png");


    // -------------------- Dugmad spratova i kontrola ----------------------
    // -------------------- Dugmad spratova (8 spratova) --------------------
    float buttonH = BUTTON_HEIGHT;
    float buttonW = BUTTON_WIDTH;
    float startX = BUTTON_START_X;           // blago ulevo
    float startY = BUTTON_START_Y;           // kreće od dna
    float spacing = BUTTON_SPACING;          // veći razmak izmedju dugmeta

    for (int i = 0; i < 8; i++) {
        float y = startY + i * spacing;
        scene.buttons.emplace_back(startX, y, buttonW, buttonH, 1, 1, 1, floorTextures[i]);
    }

    // -------------------- Kontrole vrata i sistema (rasporedjeno 2x2) --------------------
    float ctrlW = CONTROL_WIDTH, ctrlH = CONTROL_HEIGHT;
    float ctrlStartY = startY + FLOOR_COUNT * spacing + 0.05f;   // odmah iznad listi spratova

    scene.buttons.emplace_back(startX, ctrlStartY, ctrlW, ctrlH, 1, 1, 1, tex_Open);
    scene.buttons.emplace_back(startX + ctrlW + 0.02, ctrlStartY, ctrlW, ctrlH, 1, 1, 1, tex_Close);
    scene.buttons.emplace_back(startX, ctrlStartY + ctrlH + 0.02, ctrlW, ctrlH, 1, 1, 1, tex_Stop);
    scene.buttons.emplace_back(startX + ctrlW + 0.02, ctrlStartY + ctrlH + 0.02, ctrlW, ctrlH, 1, 1, 1, tex_Fan);

    // -------------------- Watermark ----------------------------------------------

    Rectangle watermark(
        WATERMARK_X,
        WATERMARK_Y,
        WATERMARK_W,
        WATERMARK_H,
        1.0f, 1.0f, 1.0f,
        tex_Watermark
    );

    // -------------------- Input Callback --------------------
    glfwSetWindowUserPointer(window, &scene);
    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            double xpos, ypos; glfwGetCursorPos(window, &xpos, &ypos);
            int width, height; glfwGetWindowSize(window, &width, &height);
            Scene* s = (Scene*)glfwGetWindowUserPointer(window);
            float nx = (xpos / width) * 2.0f - 1.0f;            // normalizacija
            float ny = 1.0f - (ypos / height) * 2.0f;
            for (size_t i = 0;i < s->buttons.size();i++) {
                Rectangle b = s->buttons[i];
                if (nx >= b.x && nx <= b.x + b.w && ny >= b.y && ny <= b.y + b.h) {
                    if (i < 8) s->pressFloorButton(i);
                    else if (i == 8) s->pressOpenDoor();
                    else if (i == 9) s->pressCloseDoor();
                    else if (i == 10) s->pressStop();
                    else if (i == 11) s->toggleVentilation();
                }
            }
        }
        });

    // -------------------- Main Loop ------------------------
    float lastTime = glfwGetTime();
    const float frameTime = 1.0f / 75.0f;

    while (!glfwWindowShouldClose(window)) {
        // frame limiter
        float currentFrame = glfwGetTime();
        float dt = currentFrame - lastTime;
        if (dt < frameTime) { glfwWaitEventsTimeout(frameTime - dt); currentFrame = glfwGetTime(); dt = currentFrame - lastTime; }
        lastTime = currentFrame;

        glClear(GL_COLOR_BUFFER_BIT);
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

        bool left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        bool right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        bool cPressed = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
        if (left) {
            scene.person.flippedPerson = false;
        } 
        if (right) {
            scene.person.flippedPerson = true;
        }

        bool left2 = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
        bool right2 = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
        bool zPressed = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
        if (left2) {
            scene.person2.flippedPerson = false;
        }
        if (right2) {
            scene.person2.flippedPerson = true;
        }
        
        bool exitKey = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
        if (exitKey) scene.startExit();

        // azuriranje pozicija
        scene.updateGenericPerson(scene.person,
            scene.personInElevator,
            dt,
            left,
            right);
        scene.updateGenericPerson(scene.person2,
            scene.person2InElevator,
            dt,
            left2,
            right2);

        scene.callElevatorFromPerson(scene.person, scene.personInElevator, glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS);
        scene.callElevatorFromPerson(scene.person2, scene.person2InElevator, glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS);
        scene.updateElevator(dt);
        scene.updateDoors(dt);

        // ventilacija kursor (obican/obojen)

        bool liftStillGoing = scene.isAnyoneInElevator() && !scene.targetFloors.empty();
        if (scene.ventilatorOn && liftStillGoing) {
            glfwSetCursor(window, cursorColor);                // obojeni propeler aktivan
        }
        else {
            glfwSetCursor(window, cursorBlack);                // crni propeler aktivan
        }

        // crtanje
         
        for (auto& f : scene.floors) renderer.drawRect(f);     // linije spratova
        renderer.drawRect(scene.elevator);
        renderer.drawElevatorDoors(scene.elevator, scene.doorOffset);
        renderer.drawRect(scene.person);
        renderer.drawRect(scene.person2);

        for (auto& b : scene.buttons) renderer.drawRect(b);
        renderer.drawRect(watermark);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

	renderer.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
