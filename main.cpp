/**
 * @file main.cpp
 * @brief Programa para manejar el estado del sistema de seguridad dentro del vehículo. Abre o cierra un relé según el estado.
 * @author Betsabe Ailen Rodriguez
 */

//=====[Librerías]===========
#include "mbed.h"

//=====[Definición de parámetros de Tiempo y función del tiempo]===========
/**
 * @def elapsed_t_s(x)
 * @brief Macro para obtener el tiempo transcurrido en segundos desde el inicio de un timer.
 * 
 * Esta macro utiliza chrono::duration_cast para convertir el tiempo transcurrido
 * desde el inicio del timer `x` en segundos.
 * 
 * @param x El objeto timer del cual se desea obtener el tiempo transcurrido.
 * @return El tiempo transcurrido en segundos como un valor entero.
 */
#define elapsed_t_s(x)    chrono::duration_cast<chrono::seconds>((x).elapsed_time()).count()

#define TIME_FOR_OVERTIME 5         ///< Tiempo en segundos para considerar sobretiempo en la comunicación
#define ALARM_TIME 20               ///< Tiempo en segundos para la alarma de aviso

//=====[Declaración e inicialización de objetos globales públicos]=============
DigitalIn button(BUTTON1, PullUp);  ///< Botón conectado al pin BUTTON1 con resistencia PullUp, correspondiente a la activación de PANIC.

DigitalOut led1(LED1);              ///< LED conectado al pin LED1, indicador de alarma 
DigitalOut relay(D12);              ///< Relé conectado al pin D12, en el sistema este desconectaria o conectaría el motor del auto 
DigitalOut buzzer(D11);             ///< Buzzer conectado al pin D11, indicador de alarma 

Timer timer;                        ///< Timer para la temporización general

static UnbufferedSerial serialComm(PB_10, PB_11); ///< Comunicación serial sin buffer en los pines PB_10 y PB_11 

//=====[Declaración e inicialización de variables globales públicas]===========
/**
 * @enum States
 * @brief Enumera los posibles estados del sistema.
 */
enum States {
    OFF,        ///< Estado apagado 
    MONITOR,    ///< Estado de monitoreo 
    PANIC       ///< Estado de pánico 
};

States currentState = OFF;          ///< Estado actual del sistema 

bool isPanicBlock = false;          ///< Indica si el estado de pánico se ha concretado o si se da prioridad al boton de PANIC

//=====[Declaraciones (prototipos) de funciones públicas]=======================
/**
 * @brief Apaga todos los dispositivos de salida (LED, relé, buzzer).
 * @param none
 * @return void
 */
void outputsOffSet();

/**
 * @brief Maneja el estado de monitoreo.
 * 
 * Apaga todas las salidas y verifica si ha transcurrido el tiempo máximo de monitoreo.
 * Si se excede el tiempo límite, transiciona al estado de pánico.
 *
 * @param none
 * @return void
 */
void handleMonitorState();

/**
 * @brief Maneja el estado de pánico.
 *
 * En este estado se manifiesta una alarma y cuando esta concluye se bloquea el estado PANIC
 * a menos que intencionalmente se apage
 * Controla el parpadeo de los LEDs y el sonido del buzzer durante el estado de pánico.
 * Si se excede el tiempo de alarma, activa el LED, desactiva el buzzer y activa el relé.
 * Además, envía una señal serial 'P' si no se ha bloqueado previamente.
 *
 * @param none
 * @return void
 */
void handlePanicState();

/**
 * @brief Procesa la comunicación serial entrante y gestiona las transiciones de estado.
 * 
 * Lee un carácter de la comunicación serial y realiza las siguientes acciones según el carácter recibido:
 * - 'o': Transiciona al estado OFF y envía 'O' por la comunicación serial.
 * - 'm': Si el estado actual es MONITOR, envía 'M' y reinicia el temporizador; de lo contrario, transiciona a MONITOR y realiza lo mismo.
 * - 'p': Transiciona al estado PANIC y envía 'P' por la comunicación serial.
 * - Otros caracteres: Si `isPanicBlock` es verdadero, envía 'P' por la comunicación serial.
 * 
 * Esta función maneja las transiciones de estado del sistema y las respuestas esperadas desde la comunicación serial.
 * Si "isPanicBlock" es verdadero y se recibe un carácter distinto de 'o', se envía 'P' por la comunicación serial.
 * La función no tiene efecto si no hay datos disponibles para lectura.
 *
 * @param none
 * @return void
 */
void processCommunication();

/**
 * @brief Procesa la presión del botón y maneja la transición de estado.
 * 
 * Esta función verifica el estado del botón y realiza las transiciones de estado
 * apropiadas basadas en la presión del botón y el estado actual del sistema.
 * 
 * - Si el botón está presionado (button == false) y "isButtonPressed" es falso y "isPanicBlock" es falso,
 *   establece "isButtonPressed" a verdadero, activa "isPanicBlock", envía 'P' por el puerto serie y
 *   transiciona el estado del sistema a PANIC. 
 * - la variable isPanicBlock en esta funcion actúa para dar prioridad a PANIC sobre otros estados entrantes con excepción de OFF
 * - Si el botón no está presionado (button == true), establece "isButtonPressed" a falso.
 * 
 * @param none
 * @return void
 */
void processButtonPress();

/**
 * @brief Transiciona el sistema a un nuevo estado.
 * 
 * Esta función actualiza el estado actual del sistema y reinicia el temporizador.
 *
 * @param newState El nuevo estado al que se transicionará (OFF, MONITOR o PANIC).
 * @return void
 */
void transitionToState(States newState);

/**
 * @brief Maneja la lógica de comunicación, la presión del botón y la transición de estados.
 *
 * Llama a las funciones para procesar la comunicación serial y la presión de botones.
 * Luego, ejecuta el manejo correspondiente según el estado actual del sistema.
 *
 * @param none
 * @return void
 */
void processStates();

//=====[Función principal, el punto de entrada del programa después de encender o resetear]========
/**
 * @brief Función principal del programa.
 *
 * Esta función inicializa los componentes necesarios, configura el puerto serial
 * y el temporizador, y luego entra en un bucle infinito donde procesa los estados
 * del sistema.
 *
 * @return int El valor de retorno representa el éxito de la aplicación.
 */
int main()
{
    // Inicialización del puerto serial
    serialComm.baud(9600);  // Configura la velocidad de baudios a 9600
    serialComm.set_blocking(false);  // Configura la comunicación serial como no bloqueante

    timer.start();  // Inicia el temporizador

    while (true) {
        processStates();  // Llamada a la función que maneja los estados del sistema
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
        if (!isPanicBlock) {
            serialComm.write("P", 1);
            isPanicBlock = true;
        }
    }
}

void processCommunication() {
    if (serialComm.readable()) {
        char ch;
        if (serialComm.read(&ch, 1) > 0) {
            if (!isPanicBlock || ch == 'o') {
                switch (ch) {
                    case 'o':
                        transitionToState(OFF);
                        serialComm.write("O", 1);
                        isPanicBlock = false;
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

    bool isButtonPressed = false;  // Variable local para la presión del botón

    if (button == 0 && !isButtonPressed && !isPanicBlock) {
        isButtonPressed = true;
        isPanicBlock = true;
        serialComm.write("P", 1);
        transitionToState(PANIC);
    } else if (button == 1) {
        isButtonPressed = false;
    }
}

void transitionToState(States newState) {
    currentState = newState;
    timer.reset();
}
