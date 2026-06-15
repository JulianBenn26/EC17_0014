#ifndef BLE_H
#define BLE_H

void UART1_Init(void);
void UART1_WriteChar(char c);
void UART1_WriteString(const char *str);
char UART1_ReadChar(void);
void UART1_ReadLine(char *buffer, int maxLen);

void BLE_SendCommand(const char *cmd);
void BLE_ReadResponse(char *response, int maxLen);
void BLE_Init(void);

#endif

