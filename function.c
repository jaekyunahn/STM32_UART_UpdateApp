/*
 *  Include
 */
#include "main.h"

/*
 *  Define
 */
#define PACKETSIZE      256
#define BUFFERSIZE      256

#define LIMIT_FILESIZE  2097152
#define LIMIT_PACKET    ((LIMIT_FILESIZE / PACKETSIZE) + 1 )

#define DEBUG           0

 /*
  *  Variable
  */
//UART RX Buffer
volatile char   Rx_buf[1024];

// ���� ���� ũ��
volatile int    iRxBufferCounter = 0;

//UART TX Buffer
volatile char   Tx_buf[BUFFERSIZE];

//������ ���� ũ��
volatile int    iSendPacketSize = 0;

// File Read Buffer
char sFileReadBuffer[LIMIT_FILESIZE];
// Command ���� �κ�
char command[32] = "";
// MCU�� ���� ���� �޼��� Type
char sMessageType[32] = "";
// MCU�� ���� ���� �޼��� ���� case
char sResponseType[32] = "";
// �ӽ� ����
char sTempStringData[BUFFERSIZE] = "";

/*
 *  @brief  File Load Function
 *  @param  sFileName   File �ּ�
 *          sReadBuffer File Data ���� �� �迭 ���� ���� �ּ�
 *  @retval �б� ���� �� File ũ�� ��ȯ, ���н� -1 return
 */
int fFileLoadFunction(unsigned char* sFileName, unsigned char* sReadBuffer) {
    unsigned int iFileSize = 0;
    FILE* fp = fopen(sFileName, "rb");
    if (fp == NULL) {
        printf("File Open Error:%s\r\n", sFileName);
        return -1;
    }
    fseek(fp, 0L, SEEK_END);		
    iFileSize = (int)ftell(fp);
    fseek(fp, 0L, SEEK_SET);		
    fread(sReadBuffer, sizeof(char), sizeof(char) * iFileSize, fp);
    fclose(fp);
    return iFileSize;
}

/*
 *  @brief  ���ڿ� ������ ���ڸ� int32������ ��ȯ
 *  @param  source  ���ڿ� ���� ���������� �ٲ� ���ڿ� ���� �ּ�
 *  @retval int32�� ��ȯ �� ��
 */
int fConvertStringToInt32(char* source)
{
    int buf = source[0] - 0x30;
    int res = 0;
    int i = 1;
    res = res + buf;
    while (1)
    {
        if ((source[i] >= 0x30) && (source[i] <= 0x39))
        {
            res = res * 10;
            buf = source[i] - 0x30;
            res = res + buf;
        }
        else
        {
            break;
        }
        i++;
    }
    return res;
}

/*
 *  @brief  CheckSum8 Xor ���� �Լ�
 *  @param  sData       ���� ��� ���ڿ� ���� �ּ�
 *          data_size   ���� ��� ���ڿ� ũ��
 *  @retval CheckSum8 ��� ��
 */
char crc_xor_calculation(char* sData, int data_size)
{
    char res = sData[0];
    for (int i = 1; i < data_size; i++)
    {
        // �� ����Ʈ�� ���ư��鼭 XOR ����
        res = res ^ sData[i];
    }
    return res;
}

/*
 *  @brief  ���ڿ� �� �Լ�
 *  @param  source ���� ���ڿ� ���� �ּ�
 *          target �񱳴�� ���ڿ� ���� �ּ�
 *          iSize  ���� ����
 *  @retval �� ��� ��� ��, ������ 0 �ٸ��� -1
 */
int fCompareFunction(char* source, char* target, int iSize)
{
    for (int i = 0; i < iSize; i++)
    {
        if (source[i] != target[i])
        {
            return -1;
        }
    }
    return 0;
}

/*
 *  @brief  MCU update ���� �� �о���� �߿��� ������ �ɰ� �迭 ����� ���� �Լ�. �ʼ���� X
 *  @param  data        �ɰ��� ���� ������
 *  @param  iPacksize   ��Ŷ ũ��
 *  @param  packcount   ��Ŷ ����
 *  @retval ���X
 */
int testcode(char sdata[LIMIT_PACKET][PACKETSIZE], int iPacksize, int packcount)
{
    int x = 0;
    FILE* fp = fopen("./bindata", "wb");
    for (x = 0; x < packcount; x++)
    {
        fwrite(sdata[x], sizeof(char), iPacksize, fp);
    }
    close(fp);
}

/*
 *  @brief  pThread ���� �Լ�, UART ��� ���
 *  @param  data ���ʽ���� ���� ���� ������ �ڷ����� define���� ����, ���� ��� X
 *  @retval ���X
 */
void* pThread_UART_Function(void* data)
{  
    //�ø�����Ʈ�� ���ŵ� ������ ũ��
    int    cnt = 0;

    // process id
    pid_t pid;   
    // thread id 
    pthread_t tid;       

    pid = getpid();
    tid = pthread_self();

    char* thread_name = (char*)data;

    printf("Start [pThread_UART_RX_Function] pid=%d\r\n", pid);

    int  fd; 
    //int ndx; 
    //int poll_state; 
    int res = 0;
    int x = 0, y = 0;

    struct termios    uart_io;  // http://neosrtos.com/docs/posix_api/termios.html
    struct pollfd     poll_fd;  // üũ�� event ������ ���� struct
    
    unsigned char sSerialPortName[32] = "";
    int iSerialPortIndex = 0;

    //retry open Serial Port
    // ��Ȥ usb to uart�� �۵��߿� ������ ttyUSB0�� �ƴ� ttyUSB1 �̷������� �����Ѵ�
loop:
    memset(sSerialPortName, 0x00, sizeof(sSerialPortName));
    sprintf(sSerialPortName, "/dev/ttyUSB%d", iSerialPortIndex);

    // �ø��� ��Ʈ�� open
    fd = open(sSerialPortName, O_RDWR | O_NOCTTY);
    // �ø�����Ʈ open error
    if (0 > fd)
    {
        printf("open error:%s\n", sSerialPortName);
        // �ø��� ��Ʈ ��ȣ�� 65535 �Ѿ��
        if (iSerialPortIndex > 65535)
        {
            //����
            return -1;
        }
        else
        {
            //count �÷��� �õ�
            iSerialPortIndex++;
            goto loop;
        }
    }

    // �ø��� ��Ʈ ��� ȯ�� ����
    memset(&uart_io, 0, sizeof(uart_io));
    /* control flags */
    uart_io.c_cflag = B115200 | CS8 | CLOCAL | CREAD;   //115200 , 8bit, �� ���� ���¸� ����, ������ ���.
    /* output flags */
    uart_io.c_oflag = 0;
     /* input flags */
    uart_io.c_lflag = 0;  
    /* control chars */
    uart_io.c_cc[VTIME] = 0;    //Ÿ�̸��� �ð��� ����
    uart_io.c_cc[VMIN] = 1;     //ead�� �� ���ϵǱ� ���� �ּ��� ���� ������ ����

    //���� ���� �Լ� ȣ��
    tcflush(fd, TCIFLUSH);              // TCIFLUSH : �Է��� ����
    // IO �Ӽ� ����
    tcsetattr(fd, TCSANOW, &uart_io);   // TCSANOW : �Ӽ��� �ٷ� �ݿ�
    // �ø��� ��Ʈ ����̽� ����̹������� ����
    fcntl(fd, F_SETFL, FNDELAY);        // F_SETFL : ���� ���¿� ���� ��带 ����

    // poll ����� ���� �غ�   
    poll_fd.fd = fd;
    poll_fd.events = POLLIN | POLLERR;          // ���ŵ� �ڷᰡ �ִ���, ������ �ִ���
    poll_fd.revents = 0;

    while (1)
    {
        // ���ŵ� ������ �ִ��� ���� Ȯ��
        cnt = read(fd, Rx_buf, 128);
        if (cnt > 0)
        {
            iRxBufferCounter = cnt;
        }
        // �۽� �� ������ ������ ����
        if (iSendPacketSize > 0)
        {
            //������ �۽�
            res = write(fd, Tx_buf, iSendPacketSize);
            //���� �Ϸ�Ǹ� ���� ���� �ʱ�ȭ
            memset(Tx_buf,0x00,sizeof(Tx_buf));
            iSendPacketSize = 0;
        }
    }
    //�ø�����Ʈ �ݱ� ->  ��ǻ� ���ѷ����� �ʿ� X
    close(fd);
}

/*
 *  @brief  pThread ���� �Լ�, MCU update ó�� ���
 *  @param  data ���ʽ���� ���� ���� ������ �ڷ����� define���� ����, ���� ��� X
 *  @retval ���X
 */
void* pThread_Update_Comunication(void* data)
{
    // process id
    pid_t pid;
    // thread id
    pthread_t tid;        

    pid = getpid();
    tid = pthread_self();

    char* thread_name = (char*)data;

    printf("Start [pThread_Update_Comunication] pid=%d\r\n", pid);

    //XOR checksum
    char checksum = 0;

    //for���� ��� �� ����
    int x = 0, y = 0, z = 0, i = 0;
    // ','��ġ
    int iCommaIndex = 0;
    // ������ ��Ŷ ����
    int iTxDataIndex = 0;
    // ������ ��Ŷ ���� ����
    int iPacketCount = 0;
    // ��Ŷ�� ������ �κ�
    int iPaketEndIndex = 0;
    //��ɾ� ������ �κ� index
    int iCommandEndIndex = 0;
    // ��Ŷ ���� ','����
    int iCommaIndex_index = 0;
    // ��Ŷ ���� ũ��
    int iDataPacketHeadSize = 0;
    //��Ŷ ������ �κ� ũ��
    int iPacketSize = PACKETSIZE - 32;
    // ������Ʈ ���� ũ��
    unsigned int iFileSize = 0;
    //�߿��� �ɰ� ������
    char sDataMemory[LIMIT_PACKET][PACKETSIZE] = { {0x00, }, };

    //�߿��� �ɰ� ������ ���� �� �迭 �ʱ�ȭ
    for (x = 0; x < LIMIT_PACKET; x++)
    {
        memset(sDataMemory[x], 0xFF, sizeof(sDataMemory[x]));
    }

    sleep(5);

    sprintf(Tx_buf, "[RESET]\r\n");
    iSendPacketSize = sizeof(Tx_buf);

    while (1)
    {
        // ���� �� �����Ͱ� ���� ����
        if (iRxBufferCounter > 0)
        {
            // ���� ���� ���� �˻� ����, 0���� �˻��� �о� ���� �����Ϳ�
            // \r�� \n�� �� �־ ��ġ ���� �ϵ��� ��������
            for (x = BUFFERSIZE; x > 0; x--)
            {
                // \r�� \n�� ���������� ������ �ɶ�
                if ((Rx_buf[x - 1] == '\r') && (Rx_buf[x] == '\n'))
                {
                    //��ɾ� ������ �κ� index �ʱ�ȭ
                    iCommandEndIndex = 0;
                    // ��Ŷ�� ������ �κ��� ���� ���� ������ \n ��ġ
                    iPaketEndIndex = x;

                    // ��ɾ� ���� �ʱ�ȭ
                    memset(command, 0x00, sizeof(command));

                    // ��ɾ� �з� �ϱ� ���� ��ɾ� ������ �κ� ã��
                    for (i = 0; i < x; i++)
                    {
                        if (Rx_buf[i] == ']')
                        {
                            //��ɾ� ������ �κ� index ���
                            iCommandEndIndex = i;
                            // ��ɾ� �����ؿ´�
                            memcpy(command, Rx_buf + 1, iCommandEndIndex - 1);
                        }
                    }
#if DEBUG
                    printf("[Server]Rx_buf=%s\r\n", Rx_buf);
                    printf("[Server]command=%s\r\n", command);
#endif
                    // ��ɾ� ����
                    // ���� ����ϴ� ���� MCU���� ���������� ������� ���� ���� Ȯ�θ� �� �ֱ� ������ ������ ������ ��ɾ���� ����
                    // [MCU]<�޴�>,<���ż�������>\r\n ���·� ������� ������ command �κ��� MCU ���� �޴��� ���� �ߴ�
                    if (fCompareFunction(command, "MCU", 3) == 0)
                    {
                        //command �κ��� ������ �κ��� �ӽ� ���۷� ����
                        memset(sTempStringData, 0x00, sizeof(sTempStringData));
                        memcpy(sTempStringData, Rx_buf + (iCommandEndIndex + 1), iPaketEndIndex - (iCommandEndIndex + 1));
#if DEBUG
                        printf("[Server]sTempStringData=%s\r\n", sTempStringData);

#endif
                        // ','�κ� ���� �ľ� ���� �ʱ�ȭ
                        iCommaIndex_index = 0;

                        // ','��ġ �ľ�
                        for (y = 0; y < iPaketEndIndex - (iCommandEndIndex + 1); y++)
                        {
                            if (sTempStringData[y] == ',')
                            {
                                iCommaIndex_index++;
                                iCommaIndex = y;
                            }
                        }
#if DEBUG
                        printf("[Server]iCommaIndex=%d\r\n", iCommaIndex);
                        printf("[Server]iCommaIndex_index=%d\r\n", iCommaIndex_index);
#endif
                        // ','���� 1���϶�
                        if (iCommaIndex_index == 1)
                        {
                            // Get Packet Size
                            memcpy(sMessageType, sTempStringData, iCommaIndex);

                            // Get Packet Count
                            memcpy(sResponseType, sTempStringData + (iCommaIndex + 1), (iCommandEndIndex - (iCommaIndex + 1)));
#if DEBUG
                            printf("[Server]sMessageType=%s\r\n", sMessageType);
                            printf("[Server]sResponseType=%s\r\n", sResponseType);
#endif

                            // MCU ��Ʈ�δ��� ���� READY ��ȣ ������
                            if (fCompareFunction(sMessageType, "READY", 5) == 0)
                            {
                                /***** Update File �о���� *****/
                                //�о�� ������ ���� �� �迭 ���� 0xFF�� �ʱ�ȭ.
                                memset(sFileReadBuffer, 0xFF, sizeof(sFileReadBuffer));

                                //���� �б� �Լ� ���Ͽ� �����Ϳ� ���� ũ�� ��������
                                iFileSize = fFileLoadFunction("./data/Nucleo-144-F429.bin", sFileReadBuffer);
                                if ((iFileSize == 0) || (iFileSize == -1))
                                {
                                    //���� �б� ���н� App�ߴ�
                                    printf("*****FW Read Error*****\r\n");
                                    return;
                                }
                                else
                                {
                                    // ���� ũ��
                                    printf("[Server]File Size=%d\r\n", iFileSize);
                                }

                                // ��Ŷ ���� = ( ���� ũ�� / ��Ŷ ũ�� ) + ������ 2ȸ
                                iPacketCount = (iFileSize / iPacketSize) + 2;

                                // ��Ŷ ũ��� ����
                                printf("[Server]iPacketSize=%d\r\n", iPacketSize);
                                printf("[Server]iPacketCount=%d\r\n", iPacketCount);

                                // MCU�� ���� �ø������ ���� ��Ŷ ����. ��Ŷ ũ��� ��Ŷ ���� ������ ������ ����
                                memset(Tx_buf, 0x00, sizeof(Tx_buf));
                                sprintf(Tx_buf, "%d,%d", iPacketSize, iPacketCount);

                                // info ��Ŷ üũ�� ���
                                checksum = crc_xor_calculation(Tx_buf, sizeof(Tx_buf));

                                // info ��Ŷ ���� �� uart thread�� ���� ������ ũ�� ����(iSendPacketSize)
                                memset(Tx_buf, 0x00, sizeof(Tx_buf));
                                sprintf(Tx_buf, "[INFO]%d,%d,%c\r\n", iPacketSize, iPacketCount, checksum);
                                iSendPacketSize = sizeof(Tx_buf);

                                for (y = 0; y < iPacketCount; y++)
                                {
                                    memcpy(sDataMemory[y], sFileReadBuffer + (iPacketSize * y), iPacketSize);
                                }

                                // MCU�� ���� �����͸� ������������ ���� �˻��ϱ� ���� ���� �ڵ�, ����Ҷ� �ʼ��� �ƴ�
#if DEBUG
                                testcode(sDataMemory, iPacketSize, iPacketCount);
#endif
                            }
                            // ��Ʈ�δ��� ���� INFO ��ȣ ������
                            else if (fCompareFunction(sMessageType, "INFO", 4) == 0)
                            {
                                // ���� ���� ���� -> ���� INFO ��Ŷ�� ���� �����̹Ƿ� 
                                // ACK�϶� ���� ������ ��Ŷ �����ؼ� �����ָ� �ȴ�
                                if (fCompareFunction(sResponseType, "ACK", 3) == 0)
                                {
                                    // Data ��Ŷ 1��
                                    iTxDataIndex = 1;

                                    //���� ������ �迭 �ʱ�ȭ
                                    memset(Tx_buf, 0x00, sizeof(Tx_buf));

                                    // ���� ������ index set
                                    sprintf(Tx_buf, "%d,", iTxDataIndex);
                                    // index ũ�� -> ������ �ڸ��� �ƴ� ���� ���̹Ƿ� ��Ŷ index �ڸ��� �ľ�
                                    iDataPacketHeadSize = strlen(Tx_buf);

                                    //������ Data �κ��� ����
                                    memcpy(Tx_buf + iDataPacketHeadSize, sDataMemory[iTxDataIndex - 1], iPacketSize);

                                    // Data ��Ŷ Checksum ���
                                    checksum = crc_xor_calculation(Tx_buf, sizeof(Tx_buf));

                                    //���� ��Ŷ �� ����
                                    memset(Tx_buf, 0x00, sizeof(Tx_buf));
                                    sprintf(Tx_buf, "[DATA]%d,", iTxDataIndex);
                                    memcpy(Tx_buf + iDataPacketHeadSize + 6, sDataMemory[iTxDataIndex - 1], iPacketSize);

                                    //������ Checksum�� ��Ŷ ���κ� ����
                                    Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 0] = ',';
                                    Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 1] = checksum;
                                    Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 2] = '\r';
                                    Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 3] = '\n';

                                    //���� ��Ŷ ������ uart thread�� ����
                                    iSendPacketSize = iDataPacketHeadSize + 6 + iPacketSize + 3 + 1;

                                    //data index counter add
                                    iTxDataIndex++;
                                }
                                // NACK�϶� INFO ��Ŷ�� ���� Checksum�� �������� �ٽ� �ѹ� �����ش�
                                else if (fCompareFunction(sResponseType, "NACK", 4) == 0)
                                {
                                    memset(Tx_buf, 0x00, sizeof(Tx_buf));
                                    sprintf(Tx_buf, "[INFO]%d,%d,%c\n", iPacketSize, iPacketCount, checksum);
                                    iSendPacketSize = sizeof(Tx_buf);
                                }
                            }
                            // ��Ʈ�δ��� ���� DATA ��ȣ ������
                            else if (fCompareFunction(sMessageType, "DATA", 4) == 0)
                            {
                                // NACK ��ȣ�� ���ŵǸ� ���� ��Ŷ ������ ���� Data index ����
                                if (fCompareFunction(sResponseType, "NACK", 4) == 0)
                                {
                                    iTxDataIndex--;
                                }

                                //���� ������ �迭 �ʱ�ȭ
                                memset(Tx_buf, 0x00, sizeof(Tx_buf));

                                // ���� ������ index set
                                sprintf(Tx_buf, "%d,", iTxDataIndex);
                                // index ũ�� -> ������ �ڸ��� �ƴ� ���� ���̹Ƿ� ��Ŷ index �ڸ��� �ľ�
                                iDataPacketHeadSize = strlen(Tx_buf);

                                //������ Data �κ��� ����
                                memcpy(Tx_buf + iDataPacketHeadSize, sDataMemory[iTxDataIndex - 1], iPacketSize);

                                // Data ��Ŷ Checksum ���
                                checksum = crc_xor_calculation(Tx_buf, sizeof(Tx_buf));

                                //���� ��Ŷ �� ����
                                memset(Tx_buf, 0x00, sizeof(Tx_buf));
                                sprintf(Tx_buf, "[DATA]%d,", iTxDataIndex);
                                memcpy(Tx_buf + iDataPacketHeadSize + 6, sDataMemory[iTxDataIndex - 1], iPacketSize);

                                //������ Checksum�� ��Ŷ ���κ� ����
                                Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 0] = ',';
                                Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 1] = checksum;
                                Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 2] = '\r';
                                Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 3] = '\n';

                                usleep(50000);

                                //���� ���� ����
                                printf("[Server]Last index = %d\r\n", iDataPacketHeadSize + 6 + iPacketSize + 3);
                                printf("[Server]crc=%2x\r\n", checksum);
                                printf("[Server]Packet=%d\r\n", iTxDataIndex);

                                //���� ��Ŷ ������ uart thread�� ����
                                iSendPacketSize = iDataPacketHeadSize + 6 + iPacketSize + 3 + 1;

                                //data index counter add
                                iTxDataIndex++;
                            }
                            //��Ʈ�δ��� ���� ������Ʈ �Ϸ� ��ȣ ������ �Ϸ� ���� ǥ��. 
                            // ���� MCU ��Ŀ���� �� ����� �ϰ� 5�ʵ��� ������Ʈ ���� ���� Ȯ�� �ϱ� ������
                            //�߰����� ���� �����ϱ� ���� 10�� ���� �ְ� �غ� ǥ�� �ϵ��� ����.  
                            //���߿� config ������ ���� ���� Ȯ�� �ؼ� ���� �ڵ� ������ �غ� ��
                            else if (fCompareFunction(sTempStringData, "END", 3) == 0)
                            {
                                printf("*****END Update*****\r\n");
                                sleep(15);
                                printf("****READY Update****\r\n");
                            }
                        }
                    }
                    // ���� ������ ó�������� �ʱ�ȭ
                    memset(Rx_buf, 0x00, sizeof(Rx_buf));
                }
            }
        }
    }
}

/*
 *  @brief  pThread �Լ� ���� �� ����
 *  @param  void
 *  @retval void
 */
void fPtheradStart(void)
{
    int thr_id;
    int status;

    pthread_t p_thread[2];

    char p1[] = "thread_UART_Thread";           
    char p2[] = "thread_Update_Comunication";   

    // UART �ۼ��� ������ ���� �� ����
    thr_id = pthread_create(&p_thread[0], NULL, pThread_UART_Function, (void*)p1);
    if (thr_id < 0)
    {
        perror("thread create error : ");
        exit(0);
    }

    pthread_detach(p_thread[0]);
    pthread_join(p_thread[0], NULL);

    // MCU ������Ʈ ó�� ������ ���� �� ����
    thr_id = pthread_create(&p_thread[1], NULL, pThread_Update_Comunication, (void*)p2);
    if (thr_id < 0)
    {
        perror("thread create error : ");
        exit(0);
    }

    pthread_detach(p_thread[1]);
    pthread_join(p_thread[1], NULL);
}




