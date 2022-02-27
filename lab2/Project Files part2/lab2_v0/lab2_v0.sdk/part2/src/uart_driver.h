/*
 * uart_driver.h
 *
 *  Created on: July 27, 2021
 *      Author: Shyama M. Gandhi
 *    Correction in MySendByte() proposed by Le Minh Dang  Winter 2022.
 *  ------------------------------------------------------------------------------------------------------------------------------
 *
 *  Overview of uart_driver.h file:
 *  This driver file is responsible for Initializing the UART, setting up the Interrupt System, Implements the ISR when a receive or sent event is triggered via UART,
 *  and implementing the four driver functions MyIsReceiveData(), MyReceiveByte(), MyIsTransmitFull() and MySendByte().
 *
 *  This is the Skeleton driver file.  void Interrupt_Handler(arguments) is the ISR routine called whenever Receive or Transmit Interrupt occurs. Inside this ISR,
 *  add the code at the provided commented section.
 *  MyIsReceiveData(), MyReceiveByte() are the two new functions for the receive direction.
 *  MyIsTransmitFull(), MySendByte() are the two new functions for the transmit direction.
 *  Please read the comments carefully at different places in this file to understand this driver in a better way.
 *
 *  ------------------------------------------------------------------------------------------------------------------------------
 *
 */

#ifndef SRC_UART_DRIVER_H_
#define SRC_UART_DRIVER_H_

#include "xil_io.h"
#include "xuartps.h" //UART definitions header file
#include "xscugic.h" //interrupt controller header file

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "initialization.h"

void Task_UART_buffer_receive(void *p);
void Task_UART_buffer_send(void *p);

QueueHandle_t xQueue_for_transmit;  //queue handle for transmit direction queue
QueueHandle_t xQueue_for_receive;  //queue handle for receive direction queue
int CountRxIrq;            //variable to count the receive side interrupts
int CountTxIrq;            //variable to count the transmit side interrupts


//PS UART Interrupt Subroutine
void Interrupt_Handler(void *CallBackRef, u32 Event, unsigned int EventData)
{
  u8 receive_buffer;
  u8 transmit_data;
  u32 mask;
  if (Event == XUARTPS_EVENT_RECV_DATA){
      BaseType_t xHigherPriorityTaskWoken;
      xHigherPriorityTaskWoken = pdFALSE;

      /*******************************************************/
      //add one line of code to increment the receive interrupt counter variable
      CountRxIrq++;
      /*******************************************************/

      /*******************************************************/
      //implement the logic to check if there are received bytes from the UART. If yes, then read them and send it to the back of the receive queue.
      //Make sure to use proper queue function in the ISR! That is use the FromISR queue methods to secure the queues for ISR.
      //the UART received byte will be read inside the u8 "receive_buffer" variable already declared for you!
      if (XUartPs_IsReceiveData(XPAR_XUARTPS_0_BASEADDR)) {
        receive_buffer = XUartPs_RecvByte(XPAR_XUARTPS_0_BASEADDR);
        xQueueSendToBackFromISR(xQueue_for_receive, &receive_buffer, &xHigherPriorityTaskWoken);
      }
      /*******************************************************/
      portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
  }
  else if (Event == XUARTPS_EVENT_SENT_DATA){
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;

    /*******************************************************/
    //add one line of code to increment the transmit interrupt counter variable
    CountTxIrq++;
    /*******************************************************/

      //case when there are bytes inside the transmit byte and TxFIFO is not Full yet!
      while (uxQueueMessagesWaitingFromISR( xQueue_for_transmit ) > 0 && !XUartPs_IsTransmitFull(XPAR_XUARTPS_0_BASEADDR)){
        /*******************************************************/
        //write two lines of code to read the data from the front of the transmit queue and then send it to the UART.
        //Make sure to use proper queue function in the ISR! That is use the FromISR queue methods to secure the queues for ISR.
        //the data is read into the variable "transmit_data" variable from the front of the transmit queue. Now, send this data to the UART using an appropriate function.
        xQueueReceiveFromISR(xQueue_for_transmit, &transmit_data, &xHigherPriorityTaskWoken);
        //XUartPs_SendByte(XPAR_XUARTPS_0_BASEADDR, transmit_data);
        XUartPs_WriteReg(XPAR_XUARTPS_0_BASEADDR, XUARTPS_FIFO_OFFSET, transmit_data);
        /*******************************************************/
      }
      portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
      if (uxQueueMessagesWaitingFromISR( xQueue_for_transmit ) <= 0){
        //read the current interrupt mask
        mask = XUartPs_GetInterruptMask(&UART);

        //Use the XUartPs_SetInterruptMask() function to disable the TEMPTY interrupt.
        //we disable the interrupt here as there is no data to transmit from the queue to the UART.
        XUartPs_SetInterruptMask(&UART, mask & ~XUARTPS_IXR_TXEMPTY);
      }


  }
  else{
      xil_printf("Nor a RECEIVE event nor a SEND event\n");
  }
}


//Be sure to protect any critical sections into these four functions by using the FreeRTOS macros taskENTER_CRITICAL() and taskEXIT_CRITICAL().

BaseType_t MyIsReceiveData( void ){

  /*******************************************************/
  //write the code to return pdTRUE if there is newly received data into the receive queue else return pdFALSE.
  if (uxQueueMessagesWaiting(xQueue_for_receive) > 0) {
    return pdTRUE;
  }
  return pdFALSE;
  /*******************************************************/
}

u8 MyReceiveByte( void ){
  u8 recv;
  taskENTER_CRITICAL();
  /*******************************************************/
  //write the code to return the 8-bit value that is present inside the received queue of the UART.
  //use the "recv" variable to read the queue value.
  xQueueReceive(xQueue_for_receive, &recv, portMAX_DELAY);
  /*******************************************************/

  taskEXIT_CRITICAL();
  return recv;
}

BaseType_t MyIsTransmitFull( void ) {

  /*******************************************************/
  //write the code to return pdTRUE if the transmit queue data structure is FULL else return pdFALSE.
  if (uxQueueSpacesAvailable(xQueue_for_transmit) == 0) {
    return pdTRUE;
  }
  return pdFALSE;
  /*******************************************************/

}

void MySendByte( u8 Data ) {
  u32 mask;
  //secure it
  taskENTER_CRITICAL();

  /*******************************************************/
  //add the code to enable TEMPTY interrupt bit. You can first read the interrupt mask and then set the interrupt mask using the XUARTPS_IXR_TXEMPTY.
  //see the ISR code for transmit Interrupt event where there is a mechanism to first read the interrupt mask and then set it as user wants.
  //But the difference is, here you have to "enable" the XUARTPS_IXR_TXEMPTY bit.! In ISR, we have disabled this bit!

  //read the current interrupt mask
  mask = XUartPs_GetInterruptMask(&UART);
  //xil_printf("mask = %d, flag = %d\n", mask, XUARTPS_IXR_TXEMPTY);
  XUartPs_SetInterruptMask(&UART, mask | XUARTPS_IXR_TXEMPTY);  // Enable the mask by "or"-ing with the defined constant.
  //xil_printf("transmit empty = %d, waiting = %d\n", XUartPs_IsTransmitEmpty(&UART), uxQueueMessagesWaiting(xQueue_for_transmit));
  /*******************************************************/

  //if transmit FIFO empty, use polling, otherwise insert to queue for interrupt method
  if (XUartPs_IsTransmitEmpty(&UART) && uxQueueMessagesWaiting(xQueue_for_transmit) == 0){
    XUartPs_WriteReg(XPAR_XUARTPS_0_BASEADDR, XUARTPS_FIFO_OFFSET, Data);
  }else if(xQueueSend( xQueue_for_transmit, &Data, 0UL) != pdPASS ){
    xil_printf("Fail to send the data\n");
  }
  taskEXIT_CRITICAL();
}

#endif /* SRC_UART_DRIVER_H_ */

