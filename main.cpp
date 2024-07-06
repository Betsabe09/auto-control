#include "mbed.h"

#define elapsed_t_s(x)    chrono::duration_cast<chrono::seconds>((x).elapsed_time()).count()

#define TIME_FOR_OVERTIME 5      ///< Tiempo en milisegundos para considerar sobretiempo en la comunicación
#define ALARM_TIME 20            ///< Tiempo en milisegundos para la alarma de aviso

//=====[Declaración e inicialización de objetos globales públicos]=============
DigitalOut led1(LED1); /**< LED conectado al pin LED1 */
DigitalOut relay(D12); /**< Relé conectado al pin D12 */
DigitalOut buzzer(D11); /**< Buzzer conectado al pin D11 */
DigitalIn button(BUTTON1, PullUp); /**< Botón conectado al pin BUTTON1 con resistencia PullUp */
static UnbufferedSerial serialComm(D1, D0); /**< Comunicación serial sin buffer en los pines PB_10 y PB_11 */

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
Timer timer; /**< Timer para manejar el estado de monitoreo y pánico */
bool buttonPressed = false; /**< Indica si el botón ha sido presionado */
bool panicBlock = false; /**< Indica si el estado de pánico ha finalizado */

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

    if (elapsed_t_s(timer) > TIME_FOR_OVERTIME) {
        transitionToState(PANIC);
    }
}

void handlePanicState() {
    int elapsed = elapsed_t_s(timer);
    if (elapsed < ALARM_TIME) {
        led1 = elapsed % 2;
        buzzer = elapsed % 2;
    } else {
        if(!panicBlock){
            serialComm.write("P", 1);
            panicBlock = true;
        }
        led1 = 1;
        buzzer = 0;
        relay = 1;
    }
}

void checkSerialInput() {
    if (serialComm.readable()) {
        char ch;
        if (serialComm.read(&ch, 1) > 0) {
            if (!panicBlock || ch == 'o'){
                switch (ch) {
                    case 'o':
                        transitionToState(OFF);
                        serialComm.write("O", 1);
                        panicBlock = false;
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
            } else {
                serialComm.write("P", 1);
            }
        }
    }
}

void checkButtonInput() {
    if (button == 0 && !buttonPressed && !panicBlock) {
        buttonPressed = true;
         panicBlock = true;
         serialComm.write("P", 1);
        transitionToState(PANIC);
    } else if (button == 1) {
        buttonPressed = false;
    }
}

void transitionToState(State newState) {
    currentState = newState;
    timer.reset(); // Reiniciar el temporizador en cada transición de estado
}
