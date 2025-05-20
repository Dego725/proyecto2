/*
;************************************************************
; Universidad del Valle de Guatemala
; IE2023: Programación de Microcontroladores
; Lab1.asm
; Autor  : Diego Alexander Rodriguez Garay
; Proyecto: Proyecto 2 Servos
; Hardware: ATmega328P
; Creado  : 01/05/2025
;************************************************************
;-----------------------------------------------------------------------
; Definición de variables y registros*/
#define F_CPU 16000000              // Define la frecuencia del CPU a 16 MHz
#define Garra_Abierta 350           // Define el valor correspondiente a la posición abierta de la garra
#define Garra_Cerrada 1000          // Define el valor correspondiente a la posición cerrada de la garra

#include <util/delay.h>             // Incluye la librería para funciones de retardo
#include <avr/io.h>                 // Incluye definiciones para manejo de registros de E/S
#include <avr/eeprom.h>             // Incluye funciones para acceso a memoria EEPROM
#include "PWM1.h"                   // Incluye funciones para PWM relacionadas con servo A
#include "ADC.h"                    // Incluye funciones para la lectura del ADC
#include "PWM0.h"                   // Incluye funciones para PWM relacionadas con servo B y otros
#include "UART.h"                   // Incluye funciones para comunicación UART

// Definición de posiciones de memoria EEPROM para guardar cada grupo de posiciones servo
const uint8_t pos1[] = {0x00, 0x02, 0x04, 0x06};    // Dirección EEPROM para posicion 1
const uint8_t pos2[] = {0x08, 0x0A, 0x0C, 0x0E};    // Dirección EEPROM para posicion 2
const uint8_t poscar[] = {0x10, 0x12, 0x14, 0x16};  // Dirección EEPROM para posicion de carga
const uint8_t posdis[] = {0x18, 0x1A, 0x1C, 0x1E};  // Dirección EEPROM para posicion de descarga

float adcValue1 = 0;            // Valor leído del ADC para primer servo
float adcValue2 = 0;            // Valor leído del ADC para segundo servo
float adcValue3 = 0;            // Valor leído del ADC para tercer actuador (PWM0_dca)
float adcValue4 = 0;            // Valor leído del ADC para cuarto actuador (PWM0_dcb)
float dutyCycle = 0;            // Variable genérica para guardar ciclo de trabajo (no usada explícitamente aquí)
char comando[32];               // Buffer para almacenamiento de comandos recibidos vía UART
char buffer[32];                // Buffer auxiliar para recepción UART
uint8_t dato = 0;               // Variable para guardar datos leídos vía UART
uint8_t online = 0;             // Indicador para modo online

// Función que guarda los valores de los servos en la EEPROM
void saveServoPositions(uint16_t EE_POS1_ADDR, uint16_t EE_POS2_ADDR, uint16_t EE_POS3_ADDR, uint16_t EE_POS4_ADDR) {
	eeprom_update_word(EE_POS1_ADDR, adcValue1);   // Guarda adcValue1 en la dirección EE_POS1_ADDR
	eeprom_update_word(EE_POS2_ADDR, adcValue2);   // Guarda adcValue2 en la dirección EE_POS2_ADDR
	eeprom_update_word(EE_POS3_ADDR, adcValue3);   // Guarda adcValue3 en la dirección EE_POS3_ADDR
	eeprom_update_word(EE_POS4_ADDR, adcValue4);   // Guarda adcValue4 en la dirección EE_POS4_ADDR
}

// Función que carga valores guardados de los servos desde EEPROM
void loadServoPositions(uint16_t EE_POS1_ADDR, uint16_t EE_POS2_ADDR, uint16_t EE_POS3_ADDR, uint16_t EE_POS4_ADDR) {
	adcValue1 = eeprom_read_word(EE_POS1_ADDR);   // Lee valor desde EE_POS1_ADDR en adcValue1
	adcValue2 = eeprom_read_word(EE_POS2_ADDR);   // Lee valor desde EE_POS2_ADDR en adcValue2
	adcValue3 = eeprom_read_word(EE_POS3_ADDR);   // Lee valor desde EE_POS3_ADDR en adcValue3
	adcValue4 = eeprom_read_word(EE_POS4_ADDR);   // Lee valor desde EE_POS4_ADDR en adcValue4
}

// Definición de estructura para comandos UART
typedef struct {
	char id[3];       // Identificador del comando, ej. "S1"
	int value;        // Valor asociado al comando, ej. 500
} ComandoUART;

// Función que parsea un comando UART recibido tipo "S1: 500"
uint8_t UART_parse_comando(const char* input, ComandoUART* cmd) {
	char* sep = strchr(input, ':');     // Busca carácter ':' en el string de entrada
	if (sep == NULL) return 0;           // Si no encuentra ':', comando inválido y retorna 0

	uint8_t len = sep - input;           // Calcula la longitud del id antes del ':'
	if (len > 2) return 0;               // Si id es más largo que 2 caracteres, comando inválido

	strncpy(cmd->id, input, len);        // Copia los primeros caracteres al id
	cmd->id[len] = '\0';                 // Agrega fin de cadena al id

	cmd->value = atoi(sep + 1);          // Convierte la parte después del ':' a valor entero

	return 1;                           // Comando parseado correctamente, retorna 1
}

int main(void) {
	DDRD = 0xFF;                        // Configura todo el puerto D como salida
	DDRC = 0b00000111;                  // Configura los bits 0,1 y 2 del puerto C como salida

	ADC_init();                        // Inicializa el ADC
	PWM1_init();                      // Inicializa el PWM1 para servos
	PWM0_init();                      // Inicializa el PWM0 para actuadores
	UART_init();                      // Inicializa UART para comunicación serial

	while (1) {                       // Bucle infinito principal
		dato = 0;                    // Reinicia variable dato a 0
		UART_write_txt("\r\n");      // Envía salto de línea por UART
		UART_write_txt("Usted se encuentra en modo manual \n\r");  // Mensaje modo manual
		UART_write_txt("- Modo EEPROM (Presionar 1) \n\r");         // Mensaje opción EEPROM
		UART_write_txt("- Modo Online (Presionar 2) \n\r");         // Mensaje opción Online

		PORTC |= (1 << PC0);          // Enciende LED o señal en PC0
		PORTC &= ~(1 << PC1);         // Apaga PC1
		PORTC &= ~(1 << PC2);         // Apaga PC2
		ADCSRA |= (1 << ADEN);        // Habilita el ADC para lectura

		while (dato == 0) {           // Bucle espera a dato para seleccionar modo
			dato = UART_read();       // Lee dato de UART

			adcValue1 = ADC_read(6);  // Lee canal ADC 6 y guarda en adcValue1
			servo_writeA(adcValue1);  // Envía valor ADC al servo A (PWM1)
			_delay_ms(5);             // Retardo breve

			adcValue2 = ADC_read(7);  // Lee canal ADC 7 y guarda en adcValue2
			servo_writeB(adcValue2);  // Envía valor ADC al servo B (PWM1)
			_delay_ms(5);             // Retardo breve

			adcValue3 = ADC_read(5);  // Lee canal ADC 5 y guarda en adcValue3
			PWM0_dca(adcValue3, NO_INVERTING);  // Control PWM0 canal A
			_delay_ms(5);             // Retardo breve

			adcValue4 = ADC_read(4);  // Lee canal ADC 4 y guarda en adcValue4
			PWM0_dcb(adcValue4, NO_INVERTING);  // Control PWM0 canal B
			_delay_ms(5);             // Retardo breve
		}

		ADCSRA &= ~(1 << ADEN);       // Deshabilita el ADC

		switch (dato) {               // Decisión según tecla recibida
			case 49:  // Caso '1' en ASCII - modo EEPROM
			PORTC &= ~(1 << PC0); // Apaga PC0
			PORTC |= (1 << PC1);  // Enciende PC1
			PORTC &= ~(1 << PC2); // Apaga PC2
			dato = 0;             // Reinicia dato

			UART_write_txt("\r\n");
			UART_write_txt("- Guardar posicion (1)  \n\r");
			UART_write_txt("- Cargar posicion o secuencia (2) \n\r");

			while (dato == 0) {  // Espera opción 1 o 2
				dato = UART_read();
			}

			switch (dato) {
				case 49:  // Guardar
				UART_write_txt("\r\n");
				UART_write_txt("- Seleccione que posicion guardar  \n\r");
				UART_write_txt("- Posicion 1 (1) \n\r");
				UART_write_txt("- Posicion 2 (2) \n\r");
				UART_write_txt("- Posicion de carga (3) \n\r");
				UART_write_txt("- Posicion de descarga (4) \n\r");
				dato = 0;

				while (dato == 0) {   // Espera selección de posición a guardar
					dato = UART_read();
				}

				switch (dato) {
					case 49:
					saveServoPositions(pos1[0], pos1[1], pos1[2], pos1[3]);  // Guarda posición 1
					UART_write_txt("\r\n");
					UART_write_txt("Posicion 1 guardada \n\r");
					break;
					case 50:
					saveServoPositions(pos2[0], pos2[1], pos2[2], pos2[3]);  // Guarda posición 2
					UART_write_txt("\r\n");
					UART_write_txt("Posicion 2 guardada \n\r");
					break;
					case 51:
					saveServoPositions(poscar[0], poscar[1], poscar[2], poscar[3]);  // Posición de carga
					UART_write_txt("\r\n");
					UART_write_txt("Posicion de carga guardada \n\r");
					break;
					case 52:
					saveServoPositions(posdis[0], posdis[1], posdis[2], posdis[3]);  // Posición de descarga
					UART_write_txt("\r\n");
					UART_write_txt("Posicion de descarga guardada \n\r");
					break;
					default:
					break;  // No hacer nada si opción inválida
				}
				break;

				case 50:  // Cargar posiciones o secuencia
				UART_write_txt("\r\n");
				UART_write_txt("- Seleccione que posicion cargar  \n\r");
				UART_write_txt("- Posicion 1 (1) \n\r");
				UART_write_txt("- Posicion 2 (2) \n\r");
				UART_write_txt("- Ejecutar secuencia (3) \n\r");
				dato = 0;

				while (dato == 0) {  // Espera selección para cargar posición o ejecutar secuencia
					dato = UART_read();
				}

				switch (dato) {
					case 49:
					loadServoPositions(pos1[0], pos1[1], pos1[2], pos1[3]);  // Carga posición 1
					UART_write_txt("\r\n");
					UART_write_txt("Posicion 1 cargada \n\r");
					break;
					case 50:
					loadServoPositions(pos2[0], pos2[1], pos2[2], pos2[3]);  // Carga posición 2
					UART_write_txt("\r\n");
					UART_write_txt("Posicion 2 cargada \n\r");
					break;

					case 51:  // Ejecutar secuencia completa
					ejecutar:
					loadServoPositions(poscar[0], poscar[1], poscar[2], poscar[3]);  // Carga posición de carga
					PWM0_dcb(adcValue4, NO_INVERTING);              // Actualiza PWM0 canal B
					servo_writeA(adcValue1);                         // Mueve servo A a adcValue1
					servo_writeB(adcValue2);                         // Mueve servo B a adcValue2
					PWM0_dca(Garra_Abierta, NO_INVERTING);          // Abre la garra (PWM0 canal A)
					UART_write_txt("\r\n");
					UART_write_txt("Posicione el objeto y envie cualquier caracter \n\r");
					dato = 0;

					while (dato == 0) {  // Espera confirmación usuario para continuar
						dato = UART_read();
					}

					PWM0_dca(Garra_Cerrada, NO_INVERTING);          // Cierra la garra
					_delay_ms(1000);                               // Espera 1 segundo

					loadServoPositions(posdis[0], posdis[1], posdis[2], posdis[3]);  // Carga posición descarga
					PWM0_dcb(adcValue4, NO_INVERTING);              // Actualiza PWM0 canal B
					servo_writeA(adcValue1);                         // Mueve servo A a adcValue1
					servo_writeB(adcValue2);                         // Mueve servo B a adcValue2
					_delay_ms(1000);                               // Espera 1 segundo
					
					PWM0_dca(Garra_Abierta, NO_INVERTING);          // Abre garra para soltar objeto
					UART_write_txt("\r\n");
					UART_write_txt("Secuencia ejecutada \n\r");
					UART_write_txt("-Para volver a ejecutar la secuencia presione 1 \n\r");
					UART_write_txt("-Para volver al modo manual presione cualquier otro caracter \n\r");
					dato = 0;

					while (dato == 0) {  // Espera opción para repetir secuencia
						dato = UART_read();
					}

					if (dato == 49) {
						goto ejecutar;  // Vuelve a ejecutar la secuencia
					}
					break;

					default:
					break; // No hacer nada si opción inválida
				}

				// Después de cargar o secuenciar, actualiza servos con valores actuales
				servo_writeA(adcValue1);
				servo_writeB(adcValue2);
				PWM0_dca(adcValue3, NO_INVERTING);
				PWM0_dcb(adcValue4, NO_INVERTING);
				UART_write_txt("Envie cualquier tecla para regresar al modo manual\n\r");
				dato = 0;

				while (dato == 0) {  // Espera confirmación para volver al modo manual
					dato = UART_read();
				}
				break;

				default:
				break;
			}
			dato = 0;  // Reinicia dato para próximo ciclo
			break;

			case 50:  // Caso '2' - modo Online (control por comandos UART)
			PORTC &= ~(1 << PC0);  // Apaga PC0
			PORTC &= ~(1 << PC1);  // Apaga PC1
			PORTC |= (1 << PC2);   // Enciende PC2 para indicar modo online

			UART_write_txt("Modo online activado\n\r");       // Mensaje modo online activado
			UART_write_txt("Ahora puede mover los sliders\n\r");

			online = 0;  // Indicador modo online activo

			while (online == 0) {  // Bucle para modo online
				ComandoUART comando;
				UART_read_txt(buffer, 32);  // Lee comando UART en buffer

				if (UART_parse_comando(buffer, &comando)) {  // Si el comando está bien parseado
					UART_write_txt("ID: ");
					UART_write_txt(comando.id);
					UART_write_txt("\nValor: ");

					char num[10];
					itoa(comando.value, num, 10);    // Convierte valor a string
					UART_write_txt(num);
					UART_write('\n');

					// Selecciona qué servo o actuador mover según comando.id
					switch (comando.id[1]) {
						case '1':
						servo_writeA(comando.value);    // Mueve servo A
						break;
						case '2':
						servo_writeB(comando.value);    // Mueve servo B
						break;
						case '3':
						PWM0_dca(comando.value, NO_INVERTING);   // PWM0 canal A
						break;
						case '4':
						PWM0_dcb(comando.value, NO_INVERTING);   // PWM0 canal B
						break;
						default:
						UART_write_txt("Modo online desactivado\n");  // ID inválido termina modo
						break;
					}
					} else {
					dato = 0;
					UART_write_txt("\r\n");
					UART_write_txt("Desea salir del modo online? \n\r");
					UART_write_txt("Si (1) \n\r");
					UART_write_txt("No (Cualquier otro caracter) \n\r");

					while (dato == 0) {  // Espera respuesta para salir modo online
						dato = UART_read();
					}

					if (dato == 49) {  // Si presiona '1', salir modo online
						online = 1;
						} else {
						online = 0;  // Continuar en modo online
					}
				}
			}
			break;

			default:
			UART_write_txt("\r\n");
			UART_write_txt("Usted se encuentra en modo manual \n\r");
			UART_write_txt("- Modo EEPROM (Presionar 1) \n\r");
			UART_write_txt("- Modo Online (Presionar 2) \n\r");
			dato = 0;  // Reinicia dato para nuevo ciclo
		}
	}
}
