//=====[Bibliotecas]===========================================================
#include "mbed.h"

//=====[Declaración e inicialización de objetos globales públicos]=============
DigitalOut led1(LED1); /**< LED conectado al pin LED1 */
DigitalOut relay(D12); /**< Relé conectado al pin D12 */
DigitalOut buzzer(D11); /**< Buzzer conectado al pin D11 */
DigitalIn button(BUTTON1, PullUp); /**< Botón conectado al pin BUTTON1 con resistencia PullUp */
static UnbufferedSerial serialComm(PB_10, PB_11); /**< Comunicación serial sin buffer en los pines PB_10 y PB_11 */

//=====[Declaración e inicialización de variables globales públicas]===========
/**
 * @enum State
 * @brief Enumera los posibles estados del sistema.
 */
enum State {
    OFF, /**< Estado apagado */
    MONITOR, /**< Estado de monitoreo */
    PANIC /**< Estado de pánico */
};

State currentState = OFF; /**< Estado actual del sistema */
Timer timer; /**< Timer para manejar el estado de monitoreo */
Timer panicTimer; /**< Timer para manejar el estado de pánico */
bool buttonPressed = false; /**< Indica si el botón ha sido presionado */

//=====[Declaraciones (prototipos) de funciones públicas]=======================
/**
 * @brief Apaga todos los dispositivos de salida (LED, relé, buzzer).
 * @param none
 */
void outputsOffSet();
/**
 * @brief Maneja el estado de monitoreo.
 * @param none
 */
void handleMonitorState();
/**
 * @brief Maneja el estado de pánico.
 * @param none
 */
void handlePanicState();
/**
 * @brief Verifica si hay entrada serial y maneja las transiciones de estado basadas en ella.
 * @param none
 */
void checkSerialInput();
/**
 * @brief Verifica la entrada del botón y maneja la transición al estado de pánico.
 * @param none
 */
void checkButtonInput();
/**
 * @brief Transiciona el sistema a un nuevo estado.
 * @param newState El nuevo estado al que se transicionará.
 */
void transitionToState(State newState);

//=====[Función principal, el punto de entrada del programa después de encender o resetear]========
/**
 * @brief Función principal que inicializa el sistema y maneja la máquina de estados.
 * Llama a funciones para inicializar los objetos de entrada y salida declarados, e 
 * implementar el comportamiento del sistema.
 * @param none
 * @return El valor de retorno representa el éxito de la aplicación.
 */
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
