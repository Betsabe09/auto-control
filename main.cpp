#include "mbed.h"

#define elapsed_t_s(x)    chrono::duration_cast<chrono::seconds>((x).elapsed_time()).count()

#define TIME_FOR_OVERTIME 5      ///< Tiempo en segundos para considerar sobretiempo en la comunicación
#define ALARM_TIME 20            ///< Tiempo en segundos para la alarma de aviso

//=====[Declaración e inicialización de objetos globales públicos]=============
DigitalOut led1(LED1); /**< LED conectado al pin LED1 */
DigitalOut relay(D12); /**< Relé conectado al pin D12 */
DigitalOut buzzer(D11); /**< Buzzer conectado al pin D11 */
DigitalIn button(BUTTON1, PullUp); /**< Botón conectado al pin BUTTON1 con resistencia PullUp */
static UnbufferedSerial serialComm(D1, D0); /**< Comunicación serial sin buffer en los pines PB_10 y PB_11 */

//=====[Declaración e inicialización de variables globales públicas]===========
enum State {
    OFF, /**< Estado apagado */
    MONITOR, /**< Estado de monitoreo */
    PANIC /**< Estado de pánico */
};

State currentState = OFF; /**< Estado actual del sistema */
Timer timer; /**< Timer para manejar el estado de monitoreo y pánico */
bool isButtonPressed = false; /**< Indica si el botón ha sido presionado */
bool panicBlock = false; /**< Indica si el estado de pánico ha finalizado */

//=====[Declaraciones (prototipos) de funciones públicas]=======================
void outputsOffSet();
void handleMonitorState();
void handlePanicState();
void processCommunication();
void processButtonPress();
void transitionToState(State newState);
void processStates();

//=====[Función principal, el punto de entrada del programa después de encender o resetear]========
int main() {
    // Inicialización
    serialComm.baud(9600);
    serialComm.set_blocking(false);

    timer.start();
    currentState = OFF;
    
    while (true) {
        processStates(); // Llamada a la nueva función que maneja los estados
    }
}

void processStates() {
    processCommunication();
    processButtonPress();

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

void outputsOffSet() {
    led1 = 0;
    relay = 0;
    buzzer = 0;
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
        led1 = 1;
        buzzer = 0;
        relay = 1;
        if (!panicBlock) {
            serialComm.write("P", 1);
            panicBlock = true;
        }
    }
}

void processCommunication() {
    if (serialComm.readable()) {
        char ch;
        if (serialComm.read(&ch, 1) > 0) {
            if (!panicBlock || ch == 'o') {
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
                        break;
                }
            } else {
                serialComm.write("P", 1);
            }
        }
    }
}

void processButtonPress() {
    if (button == 0 && !isButtonPressed && !panicBlock) {
        isButtonPressed = true;
        panicBlock = true;
        serialComm.write("P", 1);
        transitionToState(PANIC);
    } else if (button == 1) {
        isButtonPressed = false;
    }
}

void transitionToState(State newState) {
    currentState = newState;
    timer.reset();
}