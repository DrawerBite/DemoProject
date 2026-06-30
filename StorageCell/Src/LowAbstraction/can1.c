#include "LowAbstraction/can1.h"

extern volatile uint8_t CAN_HELLO; // Диагностика: сброс прямо в ISR

/*
 * CAN1
 *
 * GPIO - PA11, PA12
 *
 *
 *
 */

static void can1_GPIO_init() {
	// 1. Тактирование PA портов
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

	// 2. Настройка пинов
	/* PA11 - CAN_RX (Вход с подтяжкой вверх) */
	GPIOA->CRH	&= ~(GPIO_CRH_CNF11 | GPIO_CRH_MODE11);
	GPIOA->CRH	|= GPIO_CRH_CNF11_1;  /* CNF11 = 10, MODE11 = 00 */
	GPIOA->ODR	|= GPIO_ODR_ODR11;     /* Включение подтяжки вверх */
	/* PA12 - CAN_TX (Альтернативная функция выхода Push-pull, 50MHz) */
	GPIOA->CRH	&= ~(GPIO_CRH_CNF12 | GPIO_CRH_MODE12);
	GPIOA->CRH	|= (GPIO_CRH_CNF12_1 | GPIO_CRH_MODE12); /* CNF12 = 10, MODE12 = 11 */

	// 3.
	AFIO->MAPR &= ~AFIO_MAPR_CAN_REMAP; //CAN alternative function on PA11 and PA12
}

void can1_init() {
    // 1. Сначала инициализируем порты GPIO, чтобы избежать переходных процессов на шине
    can1_GPIO_init();

    // 2. Включаем тактирование модуля CAN1
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

    // 3. Входим в режим инициализации CAN и ждём подтверждения
    CAN1->MCR |= CAN_MCR_INRQ | CAN_MCR_ABOM;
    while (!(CAN1->MSR & CAN_MSR_INAK)) {}

    /*
     * 4. Установка скорости CAN
     *
     * Устанавливаем ширину пересинхронизации SJW = 1
     * Настраиваем предделитель скорости BRP = 4 - 1 = 3
     * Настраиваем сегмент времени TS1 = 12 - 1 = 11
     * Настраиваем сегмент времени TS2 = 3 - 1 = 2
     *
     * CAN_CLK = 16 MHz / 4 = 4 MHz
     * Bit_time = 1 + 12 + 3 = 16 tq
     * Bitrate = 4 MHz / 16 = 250 kbit/s
     */
    CAN1->BTR &= ~CAN_BTR_SJW;
    CAN1->BTR &= ~CAN_BTR_TS1;
    CAN1->BTR |= (11 << 16);
    CAN1->BTR &= ~CAN_BTR_TS2;
    CAN1->BTR |= (2 << 20);
    CAN1->BTR &= ~0x3FF;
    CAN1->BTR |= (3 << 0);

    // 5. Выходим из режима инициализации и ждём подтверждения
    CAN1->MCR &= ~CAN_MCR_INRQ;
    while (CAN1->MSR & CAN_MSR_INAK) {}

    // 6. Выходим из режима сна
    CAN1->MCR &= ~CAN_MCR_SLEEP;
    while (CAN1->MSR & CAN_MSR_SLAK) {}

    // 7. Устанавливаем ID
    CAN1->sTxMailBox[0].TIR = 0;
    CAN1->sTxMailBox[0].TIR = (CAN_MODULE_ADRESS << 21);

	// 8. Настройка фильтров
	CAN1->FMR |= CAN_FMR_FINIT; // Входим в режим инициализации фильтров
	CAN1->FFA1R &= ~(1 << 0);  // Назначаем фильтр 0 в FIFO0
	CAN1->FS1R |= (1 << 0);    // 32-битный масштаб для фильтра 0
	CAN1->FM1R &= ~(1 << 0);   // Масочный режим (принимаем всё)
	CAN1->sFilterRegister[0].FR1 = 0; // Wildcard
	CAN1->sFilterRegister[0].FR2 = 0; // Маска = 0 (принимаем все)
	CAN1->FA1R |= (1 << 0);	// Активируем фильтр 0
	CAN1->FMR &= ~CAN_FMR_FINIT; // Выходим из режима инициализации фильтров

	// 9. Инициализация прерывания FIFO0
	CAN1->IER |= CAN_IER_FMPIE0;
	NVIC_EnableIRQ(CAN1_RX0_IRQn);
	NVIC_SetPriority(CAN1_RX0_IRQn, 1);
}

void can1_tx_data(const uint8_t *data, uint8_t dlc) {
	if (dlc > 8) dlc = 8;

	// Входим в критическую секцию
	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	// Ждем освобождения почтового ящика 0 с таймаутом
	uint32_t timeout = 0xFFFF;
	while (!(CAN1->TSR & CAN_TSR_TME0)) {
		if (--timeout == 0) {
			__set_PRIMASK(primask);
			return;
		}
	}

	// 1. Формируем переменные
	uint32_t tdlr = 0;
	uint32_t tdhr = 0;
	for (uint8_t i = 0; i < dlc; i++) {
		if (i < 4) tdlr  |= data[i] << (i*8);
		else tdhr |= data[i] << ((i-4)*8);
	}

	// 2. Формируем сигнал
	CAN1->sTxMailBox[0].TDLR = tdlr;
	CAN1->sTxMailBox[0].TDHR = tdhr;
	CAN1->sTxMailBox[0].TDTR = dlc;

	// 3. Создаём запрос на передачу
	CAN1->sTxMailBox[0].TIR |= 1;

	// Выходим из критической секции
	__set_PRIMASK(primask);
}

volatile uint8_t can_rx_flag = 0;
volatile uint8_t can_rx_dlc = 0;
volatile uint8_t can_rx_buf[8] = {0};

void USB_LP_CAN_RX0_IRQHandler(void) {
    if (CAN1->RF0R & CAN_RF0R_FMP0) {

        // 1. Читаем кадр
        uint32_t RIR  = CAN1->sFIFOMailBox[0].RIR;
        uint32_t RDTR = CAN1->sFIFOMailBox[0].RDTR;
        uint32_t RDLR = CAN1->sFIFOMailBox[0].RDLR;
        uint32_t RDHR = CAN1->sFIFOMailBox[0].RDHR;

        // 2. Получаем ID (хотя на модуле мы обычно принимаем широковещательные/конкретные кадры)
        uint32_t id;
        if (RIR & CAN_RI0R_IDE) {
            id = (RIR >> 3) & 0x1FFFFFFF;
        } else {
            id = (RIR >> 21) & 0x7FF;
        }

        // 3. Кол-во байт в данных
        uint8_t dlc = RDTR & 0x0F;

        // 4. Извлечение данных прямо в буфер
        for (uint8_t i = 0; i < dlc && i < 8; i++) {
        	if (i < 4) can_rx_buf[i] = RDLR >> (i*8);
        	else can_rx_buf[i] = RDHR >> ((i-4)*8);
        }

        // 5. Выставляем флаги
        can_rx_dlc = dlc;
        can_rx_flag = 1;

        // 6. Очищаем FIFO0
        CAN1->RF0R |= CAN_RF0R_RFOM0;
    }
}

