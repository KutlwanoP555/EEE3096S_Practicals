/**
  ******************************************************************************
  * @file    task6_fsm.c
  * @brief   TASK 6 : NON-BLOCKING EEPROM TRANSACTION STATE MACHINE
  *
  * Restructure the Task 4 transaction so the main loop never stops:
  *
  *   request -> write-enable -> write -> wait for EEPROM
  *           -> read-back -> verify -> result
  *
  * Rules (handout, Task 6):
  *   - No HAL_Delay(), and no software busy-wait for the EEPROM's internal
  *     write cycle. You may use HAL_GetTick() to decide when the next status
  *     check is due.
  *   - Each call does a small amount of work, updates the state, and returns.
  *   - Do not put the whole transaction inside one blocking function called
  *     from the loop.
  *   - While a transaction is in progress the loop must still respond to PA3.
  *
  * Controls: PA0 starts, PA3 aborts. PB0..PB7 show the last byte read, PB11
  * (green) a successful verification, PB10 (red) a failed one.
  ******************************************************************************
  */

#include "prac2a.h"

/* TODO 6.1  Design your states. You need enough of them to tell apart at
 *           least: idle, write preparation, write transaction, EEPROM busy /
 *           status checking, read transaction, verification, and success or
 *           failure.
 *
 *           Draw the diagram first. It must show the initial state, the
 *           condition on every transition, and the success, failure and
 *           abort paths.
 *
 *           typedef enum { ... } ee_state_t;
 */

typedef enum {
    EE_IDLE,
    EE_WREN,
    EE_WRITE,
    EE_POLL,
    EE_READ,
    EE_VERIFY,
    EE_PASS,
    EE_FAIL
} ee_state_t;

volatile uint8_t  ee_state     = EE_IDLE;    /* uint8_t to match prac2a.h */
volatile uint8_t  ee_last_read = 0u;
volatile uint8_t  ee_use_fsm   = 1u;


static uint32_t poll_start = 0u;


//variables to add a small delay between each transition
volatile uint32_t ee_step_delay_ms = 500u;    /* 0 = full speed, set to 500+ for demo */
static uint32_t   ee_step_time     = 0u;


void update_eeprom_state_machine(uint32_t now)
{
    /* TODO 6.2  One step of your state machine.
     *
     *   - btn_start_edge (PA0) starts a transaction from idle.
     *   - btn_abort_edge (PA3) abandons the transaction in progress and returns
     *     to idle. Leave the SPI bus in a state the next transaction can use.
     *   - While the EEPROM is busy writing, do NOT wait in here. Work out when
     *     the next status check is due, remember it, and return.
     *   - Decide that the write has finished from the status register, never
     *     from elapsed time alone. Also decide what happens if the EEPROM never
     *     reports ready.
     *   - Store the byte read back in ee_last_read.
     *   - Clear each button edge once you have acted on it. */


	    /* Abort from any active state */

	//small delay for transitions
	if (ee_step_delay_ms > 0u && (uint32_t)(now - ee_step_time) < ee_step_delay_ms)
	{
	    return;    /* not time for the next step yet */
	}
	ee_step_time = now;


	    if (btn_abort_edge)
	    {
	        btn_abort_edge = 0u;
	        if (ee_state != EE_IDLE && ee_state != EE_PASS && ee_state != EE_FAIL)
	        {
	            eeprom_cs_high();          /* release the bus cleanly */
	            ee_state = EE_IDLE;
	        }
	        return;
	    }

	    switch (ee_state)

	    {
	    case EE_IDLE:
	        if (btn_start_edge)
	        {
	            btn_start_edge = 0u;
	            ee_state = EE_WREN;
	        }
	        break;

	    case EE_WREN:
	        eeprom_cs_low();
	        spi_transfer(EEPROM_CMD_WREN);
	        eeprom_cs_high();
	        ee_state = EE_WRITE;
	        break;

	    case EE_WRITE:
	        eeprom_cs_low();
	        spi_transfer(EEPROM_CMD_WRITE);
	        spi_transfer((uint8_t)(eeprom_test_addr >> 8));
	        spi_transfer((uint8_t)(eeprom_test_addr & 0xFF));
	        spi_transfer(eeprom_test_byte);
	        eeprom_cs_high();              /* starts the internal write */
	        poll_start = now;
	        ee_state = EE_POLL;
	        break;

	    case EE_POLL:
	        if (!(eeprom_read_status() & EEPROM_SR_RDY))
	        {
	            /* Write finished */
	            eeprom_write_wait_ms = (uint32_t)(now - poll_start);
	            ee_state = EE_READ;
	        }
	        else if ((uint32_t)(now - poll_start) >= EEPROM_WRITE_TIMEOUT_MS)
	        {
	            /* Timeout */
	            eeprom_timeout_count++;
	            ee_state = EE_FAIL;
	        }
	        /* else: still busy, just return — main loop keeps running */
	        break;

	    case EE_READ:
	        eeprom_cs_low();
	        spi_transfer(EEPROM_CMD_READ);
	        spi_transfer((uint8_t)(eeprom_test_addr >> 8));
	        spi_transfer((uint8_t)(eeprom_test_addr & 0xFF));
	        ee_last_read = spi_transfer(0x00u);
	        eeprom_cs_high();
	        ee_state = EE_VERIFY;
	        break;

	    case EE_VERIFY:
	        eeprom_verify_ok = (ee_last_read == eeprom_test_byte) ? 1u : 0u;
	        ee_state = eeprom_verify_ok ? EE_PASS : EE_FAIL;
	        break;

	    case EE_PASS:
	    case EE_FAIL:
	        /* Stay here until a new PA0 press */
	        if (btn_start_edge)
	        {
	            btn_start_edge = 0u;
	            ee_state = EE_WREN;
	        }
	        break;
	    }


}

void update_outputs(void)
{
    /* TODO 6.3  PB0..PB7 show ee_last_read (leds_write_byte). Green after a
     *           successful verification, red after a failed one
     *           (status_leds_show). Decide what the status LEDs should show
     *           while a transaction is in progress, and after an abort. */

	    leds_write_byte(ee_last_read);

	    if (ee_state == EE_PASS)
	        status_leds_show(STATUS_PASS);
	    else if (ee_state == EE_FAIL)
	        status_leds_show(STATUS_FAIL);
	}



/* ==========================================================================
 * RUN_TASK 6 - given
 *
 * The main loop has exactly the shape the handout asks for:
 *     read_inputs();  update_eeprom_state_machine();  update_outputs();
 *
 * Set ee_use_fsm = 0 in Live Expressions to run the Task 4 blocking path on
 * PA0 instead - useful when you explain why yours is non-blocking. PC13 keeps
 * toggling as a heartbeat; watch it on the scope in both modes.
 * ========================================================================== */

void task6_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
    board_io_init();

    eeprom_read_only_path();
    ee_last_read = eeprom_read_value;

    /* TODO 6.4  Your state machine starts in its idle state. Make sure the
     *           boot-time result (eeprom_verify_ok) still shows on the status
     *           LEDs, so a reset still shows green for the persistence test. */


    ee_state = EE_IDLE;
    update_outputs();

}

void task6_loop(uint32_t now)
{
    task1_gpio_update(now);
    read_inputs(now);

    if (ee_use_fsm)
    {
        update_eeprom_state_machine(now);
        update_outputs();
    }
    else
    {
        if (btn_start_edge)
        {
            btn_start_edge = 0u;
            eeprom_write_verify_path();     /* Task 4: blocks for the write */
            ee_last_read = eeprom_read_value;
        }
        btn_abort_edge = 0u;
    }
}

