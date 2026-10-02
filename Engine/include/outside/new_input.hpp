#pragma once

#define MAXKEYS 512
#define MAXBUTTONS 8

#include <glm/glm.hpp>

class newInput
{
public:

    newInput() = default;
    ~newInput() = default;

    void initialize();

    /// <summary>
    /// Update the input
    /// </summary>
    void updateInput();

    enum Key
    {
        SPACEBAR = 32,

        APOSTROPHE = 39,  // '
        COMMA = 44,
        MINUS = 45,
        PERIOD = 46,
        SLASH = 47,

        NUM_0 = 48,
        NUM_1 = 49,
        NUM_2 = 50,
        NUM_3 = 51,
        NUM_4 = 52,
        NUM_5 = 53,
        NUM_6 = 54,
        NUM_7 = 55,
        NUM_8 = 56,
        NUM_9 = 57,

        SEMICOLON = 59,  // ;
        EQUAL = 61,

        A = 65,
        B = 66,
        C = 67,
        D = 68,
        E = 69,
        F = 70,
        G = 71,
        H = 72,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        M = 77,
        N = 78,
        O = 79,
        P = 80,
        Q = 81,
        R = 82,
        S = 83,
        T = 84,
        U = 85,
        V = 86,
        W = 87,
        X = 88,
        Y = 89,
        Z = 90,

        LEFT_BRACKET = 91,
        BACKSLASH = 92,
        RIGHT_BRACKET = 93,
        GRAVE = 96,  // `

        ESCAPE = 256,
        ENTER = 257,
        TAB = 258,
        BACKSPACE = 259,
        INSERT = 260,
        DELETE_KEY = 261,
        RIGHT_ARROW = 262,
        LEFT_ARROW = 263,
        DOWN_ARROW = 264,
        UP_ARROW = 265,
        PAGE_UP = 266,
        PAGE_DOWN = 267,
        HOME = 268,
        END = 269,

        CAPS_LOCK = 280,
        SCROLL_LOCK = 281,
        NUM_LOCK = 282,
        PRINT_SCREEN = 283,
        PAUSE = 284,

        F1 = 290,
        F2 = 291,
        F3 = 292,
        F4 = 293,
        F5 = 294,
        F6 = 295,
        F7 = 296,
        F8 = 297,
        F9 = 298,
        F10 = 299,
        F11 = 300,
        F12 = 301,

        LEFT_SHIFT = 340,
        LEFT_CONTROL = 341,
        LEFT_ALT = 342,
        LEFT_SUPER = 343,
        RIGHT_SHIFT = 344,
        RIGHT_CONTROL = 345,
        RIGHT_ALT = 346,
        RIGHT_SUPER = 347,
        MENU = 348
    };

    enum MouseButton
    {
        MOUSE_LEFT = 0,
        MOUSE_RIGHT = 1,
        MOUSE_MIDDLE = 2,
        MOUSE_BUT1 = 3,
        MOUSE_BUT2 = 4,
        MOUSE_BUT3 = 5,
        MOUSE_BUT4 = 6,
        MOUSE_BUT5 = 7,
        MOUSE_BUT6 = 8,
    };

    bool keyDown(Key keyDown) { return pressedKeysHold[int(keyDown)]; }
    bool keyDownOnce(Key keyDown) { return pressedKeysOnce[int(keyDown)]; }

    bool mouseDown(MouseButton butDown) { return mouseButHold[int(butDown)]; }
    bool mouseOnce(MouseButton butDown) { return mouseButOnce[int(butDown)]; }

    float mouseScroll();

    glm::vec2 getMousePos();

    // Get relative mouse position to default size
    glm::vec2 getRelativeMousePos();


private:

    // Variables used to change input
    bool pressedKeysOnce[MAXKEYS]{false};
    bool pressedKeysHold[MAXKEYS]{false};


    bool mouseButOnce[MAXBUTTONS]{false};
    bool mouseButHold[MAXBUTTONS]{false};

    int m_scrWidth{0};
    int m_scrHeight{0};
    int m_initialScrWidth{0};
    int m_initialScrHeight{0};
};

