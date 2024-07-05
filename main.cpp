#include "mbed.h"

// Definición de pines
DigitalOut led1(LED1);
DigitalOut relay(D12);  // Asumiendo que el relé está conectado al pin D7
DigitalOut buzzer(D11); // Asumiendo que el buzzer está conectado al pin D6
DigitalIn button(BUTTON1, PullUp);

static UnbufferedSerial serialComm(PB_10, PB_11);

enum State {
    OFF,
    MONITOR,
    PANIC
};

State currentState = OFF;
Timer timer;
Timer panicTimer;
bool buttonPressed = false;

void outputsOffSet();
void handleMonitorState();
void handlePanicState();
void checkSerialInput();
void checkButtonInput();
void transitionToState(State newState);

int main() {
    // Inicialización
    serialComm.baud(9600);
    serialComm.format(8, BufferedSerial::None, 1);
    serialComm.set_blocking(false);

    timer.start();
    currentState = OFF;
    
    while (true) {
        checkSerialInput();
        checkButtonInput();

        switch (currentState) {
            case OFF:
                outputsOffSet();
                break;
            case MONITOR:
                handleMonitorState();
                break;
            case PANIC:
                handlePanicState();
                break;
        }
    }
}

void outputsOffSet() {
    led1 = 0;
    relay = 0;
    buzzer = 0;

    // Aquí la respuesta se maneja en checkSerialInput
}

void handleMonitorState() {
    outputsOffSet();

    if (chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time()).count() > 5000) {
        transitionToState(PANIC);
    }
}

void handlePanicState() {
    if (!panicTimer.elapsed_time().count()) {
        panicTimer.start();
    }

    int elapsed = chrono::duration_cast<chrono::seconds>(panicTimer.elapsed_time()).count();
    if (elapsed < 20) {
        led1 = elapsed % 2;
        buzzer = elapsed % 2;
    } else {
        led1 = 1;
        buzzer = 0;
        relay = 1;
    }
}

void checkSerialInput() {
    if (serialComm.readable()) {
        char ch;
        if (serialComm.read(&ch, 1) > 0) {
            switch (ch) {
                case 'o':
                    transitionToState(OFF);
                    serialComm.write("O", 1);
                    break;
                case 'm':
                    if (currentState == MONITOR) {
                        serialComm.write("M", 1);
                        timer.reset();
                    } else {
                        transitionToState(MONITOR);
                        serialComm.write("M", 1);
                        timer.reset();
                    }
                    break;
                case 'p':
                    transitionToState(PANIC);
                    serialComm.write("P", 1);
                    break;
                default:
                    // No se hace nada si el carácter no es reconocido
                    break;
            }
        }
    }
}

void checkButtonInput() {
    if (button == 0 && !buttonPressed) {
        buttonPressed = true;
        transitionToState(PANIC);
    } else if (button == 1) {
        buttonPressed = false;
    }
}

void transitionToState(State newState) {
    currentState = newState;
    timer.reset();
    panicTimer.reset();
}