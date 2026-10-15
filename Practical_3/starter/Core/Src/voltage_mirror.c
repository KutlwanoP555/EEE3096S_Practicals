/* EEE3096S Practical 3: complete the four TODOs in this file.
 * See ../../../docs/prac_03.md and ST RM0091, ADC and DAC chapters.
 * Use CMSIS register names; clock, GPIO and startup helpers are supplied.
 */
#include "voltage_mirror.h"
#include "board_io.h"
#include "main.h"

bool voltage_mirror_init(void)
{
    if (!board_converters_prepare()) return false;

    /* TODO 1: ADC1: 12-bit, right-aligned, software-started single conversions,
     * channel 5 only, sampling time 239.5 ADC cycles.
     * DAC channel 1: buffer on, trigger off, initial command 2048, enable.
     * Set ADC1->CFGR1, ADC1->CHSELR, ADC1->SMPR, DAC->DHR12R1 and DAC->CR.
     * Finish by returning board_adc_enable(). Remove the false return below.

    return false;  The uncompleted starter intentionally does not acquire. */

    if (!board_converters_prepare()) return false;/*Supplied helper. sets up the ADC clock and the self-calibration.*/

    /* ---- ADC1 configuration ---- */
    ADC1->CFGR1 &= ~(ADC_CFGR1_RES | ADC_CFGR1_ALIGN |ADC_CFGR1_EXTEN | ADC_CFGR1_CONT);
    ADC1->CHSELR = ADC_CHSELR_CHSEL5;//Setting only bit 5 means only channel 5 is converted. Channel 5 is PA5, our input pin.
    ADC1->SMPR = ADC_SMPR_SMP;/*ADC_SMPR_SMP sets all 3 sample time bits (111), which means 239.5 ADC clock cycles.*/

    /* ---- DAC channel 1 configuration ---- */
    DAC->DHR12R1 = 2048u;/*2048 is mid-scale, so the output starts at about half of V_R, roughly 1.65 V. That is a safe value before the first real sample*/
    DAC->CR &= ~(DAC_CR_BOFF1 | DAC_CR_TEN1);/*Trigger off means our DAC value moves to the output register automatically from the holding register, about one clock cycle after you write it.So a write to the holding register DHR12R1 changes PA4 almost immediately.*/
    DAC->CR |= DAC_CR_EN1; /*Sets the enable bit for channel 1. This is done last so the DAC only starts once its configuration is correct.*/

    return board_adc_enable();

}


bool voltage_mirror_read(uint16_t *code)
{
    /* TODO 2: reject a null pointer; clear EOC/EOS/OVR flags with W1C writes;
     * start one conversion, wait for EOC with a bounded loop, read ADC1->DR.
     * Return true only when *code contains a real acquired 12-bit result.
     * On timeout request ADSTP, then return false. Never wait forever.
     */

	    if (code == 0) return false;

	    /* Clear old flags: write 1 to clear (W1C) */
	    ADC1->ISR = ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;

	    /* Start one conversion */
	    ADC1->CR |= ADC_CR_ADSTART;

	    /* Bounded wait for end of conversion */
	    uint32_t timeout = 5000u;
	    while ((ADC1->ISR & ADC_ISR_EOC) == 0u)
	    {
	        if (--timeout == 0u)
	        {
	            ADC1->CR |= ADC_CR_ADSTP;   /* stop the stuck conversion */
	            return false;
	        }
	    }

	    *code = (uint16_t)(ADC1->DR & 0x0FFFu);
	    return true;


}

void voltage_mirror_write(uint16_t code)
{
    /* TODO 3: limit code to 0..4095 and write DAC->DHR12R1. */

	    if (code > 4095u) code = 4095u;
	    DAC->DHR12R1 = code;

}

uint16_t voltage_mirror_map(uint16_t code, bool coarse)
{
    /* TODO 4: limit code to 0..4095. FULL passes it through; COARSE keeps
     * the six most significant bits and clears the six least significant.
     * Retain the 12-bit DAC scale: do not send a 0..63 code directly.
     */

	    if (code > 4095u) code = 4095u;
	    if (coarse) code &= 0x0FC0u;  //0b0000111111000000
	    return code;

}
