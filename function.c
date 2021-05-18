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

// 수신 버퍼 크기
volatile int    iRxBufferCounter = 0;

//UART TX Buffer
volatile char   Tx_buf[BUFFERSIZE];

//전송할 버퍼 크기
volatile int    iSendPacketSize = 0;

// File Read Buffer
char sFileReadBuffer[LIMIT_FILESIZE];
// Command 저장 부분
char command[32] = "";
// MCU로 부터 받은 메세지 Type
char sMessageType[32] = "";
// MCU로 부터 받은 메세지 응답 case
char sResponseType[32] = "";
// 임시 변수
char sTempStringData[BUFFERSIZE] = "";

/*
 *  @brief  File Load Function
 *  @param  sFileName   File 주소
 *          sReadBuffer File Data 적재 할 배열 변수 시작 주소
 *  @retval 읽기 성공 시 File 크기 반환, 실패시 -1 return
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
 *  @brief  문자열 형태의 숫자를 int32값으로 변환
 *  @param  source  문자열 에서 정수형으로 바꿀 문자열 시작 주소
 *  @retval int32로 변환 된 값
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
 *  @brief  CheckSum8 Xor 연산 함수
 *  @param  sData       연산 대상 문자열 시작 주소
 *          data_size   연산 대상 문자열 크기
 *  @retval CheckSum8 결과 값
 */
char crc_xor_calculation(char* sData, int data_size)
{
    char res = sData[0];
    for (int i = 1; i < data_size; i++)
    {
        // 한 바이트씩 돌아가면서 XOR 연산
        res = res ^ sData[i];
    }
    return res;
}

/*
 *  @brief  문자열 비교 함수
 *  @param  source 비교할 문자열 시작 주소
 *          target 비교대상 문자열 시작 주소
 *          iSize  비교할 길이
 *  @retval 비교 대상 결과 값, 같으면 0 다르면 -1
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
 *  @brief  MCU update 실행 시 읽어오는 펌웨어 파일을 쪼갠 배열 디버깅 위한 함수. 필수사용 X
 *  @param  data        쪼개진 파일 데이터
 *  @param  iPacksize   패킷 크기
 *  @param  packcount   패킷 개수
 *  @retval 사용X
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
 *  @brief  pThread 실행 함수, UART 통신 담당
 *  @param  data 최초실행시 전달 받을 값으로 자료형식 define하지 않음, 현재 사용 X
 *  @retval 사용X
 */
void* pThread_UART_Function(void* data)
{  
    //시리얼포트로 수신된 데이터 크기
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
    struct pollfd     poll_fd;  // 체크할 event 정보를 갖는 struct
    
    unsigned char sSerialPortName[32] = "";
    int iSerialPortIndex = 0;

    //retry open Serial Port
    // 간혹 usb to uart를 작동중에 뽑으면 ttyUSB0이 아닌 ttyUSB1 이런식으로 증가한다
loop:
    memset(sSerialPortName, 0x00, sizeof(sSerialPortName));
    sprintf(sSerialPortName, "/dev/ttyUSB%d", iSerialPortIndex);

    // 시리얼 포트를 open
    fd = open(sSerialPortName, O_RDWR | O_NOCTTY);
    // 시리얼포트 open error
    if (0 > fd)
    {
        printf("open error:%s\n", sSerialPortName);
        // 시리얼 포트 번호가 65535 넘어가면
        if (iSerialPortIndex > 65535)
        {
            //종료
            return -1;
        }
        else
        {
            //count 올려서 시도
            iSerialPortIndex++;
            goto loop;
        }
    }

    // 시리얼 포트 통신 환경 설정
    memset(&uart_io, 0, sizeof(uart_io));
    /* control flags */
    uart_io.c_cflag = B115200 | CS8 | CLOCAL | CREAD;   //115200 , 8bit, 모뎀 라인 상태를 무시, 수신을 허용.
    /* output flags */
    uart_io.c_oflag = 0;
     /* input flags */
    uart_io.c_lflag = 0;  
    /* control chars */
    uart_io.c_cc[VTIME] = 0;    //타이머의 시간을 설정
    uart_io.c_cc[VMIN] = 1;     //ead할 때 리턴되기 위한 최소의 문자 개수를 지정

    //라인 제어 함수 호출
    tcflush(fd, TCIFLUSH);              // TCIFLUSH : 입력을 비운다
    // IO 속성 선택
    tcsetattr(fd, TCSANOW, &uart_io);   // TCSANOW : 속성을 바로 반영
    // 시리얼 포트 디바이스 드라이버파일을 제어
    fcntl(fd, F_SETFL, FNDELAY);        // F_SETFL : 파일 상태와 접근 모드를 설정

    // poll 사용을 위한 준비   
    poll_fd.fd = fd;
    poll_fd.events = POLLIN | POLLERR;          // 수신된 자료가 있는지, 에러가 있는지
    poll_fd.revents = 0;

    while (1)
    {
        // 수신된 데이터 있는지 정보 확인
        cnt = read(fd, Rx_buf, 128);
        if (cnt > 0)
        {
            iRxBufferCounter = cnt;
        }
        // 송신 할 데이터 있으면 전송
        if (iSendPacketSize > 0)
        {
            //데이터 송신
            res = write(fd, Tx_buf, iSendPacketSize);
            //전송 완료되면 전송 버퍼 초기화
            memset(Tx_buf,0x00,sizeof(Tx_buf));
            iSendPacketSize = 0;
        }
    }
    //시리얼포트 닫기 ->  사실상 무한루프라 필요 X
    close(fd);
}

/*
 *  @brief  pThread 실행 함수, MCU update 처리 담당
 *  @param  data 최초실행시 전달 받을 값으로 자료형식 define하지 않음, 현재 사용 X
 *  @retval 사용X
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

    //for문에 사용 할 변수
    int x = 0, y = 0, z = 0, i = 0;
    // ','위치
    int iCommaIndex = 0;
    // 데이터 패킷 순번
    int iTxDataIndex = 0;
    // 데이터 패킷 보낼 수량
    int iPacketCount = 0;
    // 패킷의 마지막 부분
    int iPaketEndIndex = 0;
    //명령어 마지막 부분 index
    int iCommandEndIndex = 0;
    // 패킷 내부 ','개수
    int iCommaIndex_index = 0;
    // 패킷 순번 크기
    int iDataPacketHeadSize = 0;
    //패킷 데이터 부분 크기
    int iPacketSize = PACKETSIZE - 32;
    // 업데이트 파일 크기
    unsigned int iFileSize = 0;
    //펌웨어 쪼갠 데이터
    char sDataMemory[LIMIT_PACKET][PACKETSIZE] = { {0x00, }, };

    //펌웨어 쪼갠 데이터 저장 할 배열 초기화
    for (x = 0; x < LIMIT_PACKET; x++)
    {
        memset(sDataMemory[x], 0xFF, sizeof(sDataMemory[x]));
    }

    sleep(5);

    sprintf(Tx_buf, "[RESET]\r\n");
    iSendPacketSize = sizeof(Tx_buf);

    while (1)
    {
        // 수신 된 데이터가 존재 여부
        if (iRxBufferCounter > 0)
        {
            // 상위 버퍼 부터 검색 시작, 0부터 검색시 읽어 들인 데이터에
            // \r과 \n이 들어가 있어서 골치 아픈 일들이 벌어졌다
            for (x = BUFFERSIZE; x > 0; x--)
            {
                // \r과 \n이 순차적으로 검출이 될때
                if ((Rx_buf[x - 1] == '\r') && (Rx_buf[x] == '\n'))
                {
                    //명령어 마지막 부분 index 초기화
                    iCommandEndIndex = 0;
                    // 패킷의 마지막 부분은 현재 수신 버퍼의 \n 위치
                    iPaketEndIndex = x;

                    // 명령어 버퍼 초기화
                    memset(command, 0x00, sizeof(command));

                    // 명령어 분류 하기 위해 명령어 마지막 부분 찾기
                    for (i = 0; i < x; i++)
                    {
                        if (Rx_buf[i] == ']')
                        {
                            //명령어 마지막 부분 index 잡고
                            iCommandEndIndex = i;
                            // 명령어 복사해온다
                            memcpy(command, Rx_buf + 1, iCommandEndIndex - 1);
                        }
                    }
#if DEBUG
                    printf("[Server]Rx_buf=%s\r\n", Rx_buf);
                    printf("[Server]command=%s\r\n", command);
#endif
                    // 명령어 구별
                    // 현재 사용하는 앱은 MCU에서 고정적으로 날라오는 수신 여부 확인만 해 주기 때문에 별도로 복잡한 명령어들은 없고
                    // [MCU]<메뉴>,<수신성공여부>\r\n 형태로 날라오기 때문에 command 부분은 MCU 단일 메뉴로 지정 했다
                    if (fCompareFunction(command, "MCU", 3) == 0)
                    {
                        //command 부분을 제외한 부분을 임시 버퍼로 복사
                        memset(sTempStringData, 0x00, sizeof(sTempStringData));
                        memcpy(sTempStringData, Rx_buf + (iCommandEndIndex + 1), iPaketEndIndex - (iCommandEndIndex + 1));
#if DEBUG
                        printf("[Server]sTempStringData=%s\r\n", sTempStringData);

#endif
                        // ','부분 개수 파악 변수 초기화
                        iCommaIndex_index = 0;

                        // ','위치 파악
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
                        // ','개수 1개일때
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

                            // MCU 부트로더로 부터 READY 신호 받으면
                            if (fCompareFunction(sMessageType, "READY", 5) == 0)
                            {
                                /***** Update File 읽어오기 *****/
                                //읽어롱 데이터 저장 할 배열 변수 0xFF로 초기화.
                                memset(sFileReadBuffer, 0xFF, sizeof(sFileReadBuffer));

                                //파일 읽기 함수 통하여 데이터와 파일 크기 가져오기
                                iFileSize = fFileLoadFunction("./data/Nucleo-144-F429.bin", sFileReadBuffer);
                                if ((iFileSize == 0) || (iFileSize == -1))
                                {
                                    //파일 읽기 실패시 App중단
                                    printf("*****FW Read Error*****\r\n");
                                    return;
                                }
                                else
                                {
                                    // 파일 크기
                                    printf("[Server]File Size=%d\r\n", iFileSize);
                                }

                                // 패킷 개수 = ( 파일 크기 / 패킷 크기 ) + 여유분 2회
                                iPacketCount = (iFileSize / iPacketSize) + 2;

                                // 패킷 크기와 개수
                                printf("[Server]iPacketSize=%d\r\n", iPacketSize);
                                printf("[Server]iPacketCount=%d\r\n", iPacketCount);

                                // MCU에 보낼 시리얼통신 정보 패킷 생성. 패킷 크기와 패킷 내부 데이터 사이즈 정보
                                memset(Tx_buf, 0x00, sizeof(Tx_buf));
                                sprintf(Tx_buf, "%d,%d", iPacketSize, iPacketCount);

                                // info 패킷 체크섬 계산
                                checksum = crc_xor_calculation(Tx_buf, sizeof(Tx_buf));

                                // info 패킷 구성 후 uart thread에 보낼 데이터 크기 전달(iSendPacketSize)
                                memset(Tx_buf, 0x00, sizeof(Tx_buf));
                                sprintf(Tx_buf, "[INFO]%d,%d,%c\r\n", iPacketSize, iPacketCount, checksum);
                                iSendPacketSize = sizeof(Tx_buf);

                                for (y = 0; y < iPacketCount; y++)
                                {
                                    memcpy(sDataMemory[y], sFileReadBuffer + (iPacketSize * y), iPacketSize);
                                }

                                // MCU에 보낼 데이터를 서버기준으로 따로 검사하기 위해 만든 코드, 통신할때 필수는 아님
#if DEBUG
                                testcode(sDataMemory, iPacketSize, iPacketCount);
#endif
                            }
                            // 부트로더로 부터 INFO 신호 받으면
                            else if (fCompareFunction(sMessageType, "INFO", 4) == 0)
                            {
                                // 현재 응답 상태 -> 이전 INFO 패킷에 대한 응답이므로 
                                // ACK일때 최초 데이터 패킷 생성해서 보내주면 된다
                                if (fCompareFunction(sResponseType, "ACK", 3) == 0)
                                {
                                    // Data 패킷 1번
                                    iTxDataIndex = 1;

                                    //보낼 데이터 배열 초기화
                                    memset(Tx_buf, 0x00, sizeof(Tx_buf));

                                    // 보낼 데이터 index set
                                    sprintf(Tx_buf, "%d,", iTxDataIndex);
                                    // index 크기 -> 고정된 자리가 아닌 가변 길이므로 패킷 index 자리수 파악
                                    iDataPacketHeadSize = strlen(Tx_buf);

                                    //나머지 Data 부분을 붙임
                                    memcpy(Tx_buf + iDataPacketHeadSize, sDataMemory[iTxDataIndex - 1], iPacketSize);

                                    // Data 패킷 Checksum 계산
                                    checksum = crc_xor_calculation(Tx_buf, sizeof(Tx_buf));

                                    //최종 패킷 재 구성
                                    memset(Tx_buf, 0x00, sizeof(Tx_buf));
                                    sprintf(Tx_buf, "[DATA]%d,", iTxDataIndex);
                                    memcpy(Tx_buf + iDataPacketHeadSize + 6, sDataMemory[iTxDataIndex - 1], iPacketSize);

                                    //나머지 Checksum과 패킷 끝부분 구성
                                    Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 0] = ',';
                                    Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 1] = checksum;
                                    Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 2] = '\r';
                                    Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 3] = '\n';

                                    //보낼 패킷 사이즈 uart thread로 전달
                                    iSendPacketSize = iDataPacketHeadSize + 6 + iPacketSize + 3 + 1;

                                    //data index counter add
                                    iTxDataIndex++;
                                }
                                // NACK일때 INFO 패킷에 대한 Checksum이 꼬였으니 다시 한번 보내준다
                                else if (fCompareFunction(sResponseType, "NACK", 4) == 0)
                                {
                                    memset(Tx_buf, 0x00, sizeof(Tx_buf));
                                    sprintf(Tx_buf, "[INFO]%d,%d,%c\n", iPacketSize, iPacketCount, checksum);
                                    iSendPacketSize = sizeof(Tx_buf);
                                }
                            }
                            // 부트로더로 부터 DATA 신호 받으면
                            else if (fCompareFunction(sMessageType, "DATA", 4) == 0)
                            {
                                // NACK 신호가 수신되면 이전 패킷 재전송 위해 Data index 감소
                                if (fCompareFunction(sResponseType, "NACK", 4) == 0)
                                {
                                    iTxDataIndex--;
                                }

                                //보낼 데이터 배열 초기화
                                memset(Tx_buf, 0x00, sizeof(Tx_buf));

                                // 보낼 데이터 index set
                                sprintf(Tx_buf, "%d,", iTxDataIndex);
                                // index 크기 -> 고정된 자리가 아닌 가변 길이므로 패킷 index 자리수 파악
                                iDataPacketHeadSize = strlen(Tx_buf);

                                //나머지 Data 부분을 붙임
                                memcpy(Tx_buf + iDataPacketHeadSize, sDataMemory[iTxDataIndex - 1], iPacketSize);

                                // Data 패킷 Checksum 계산
                                checksum = crc_xor_calculation(Tx_buf, sizeof(Tx_buf));

                                //최종 패킷 재 구성
                                memset(Tx_buf, 0x00, sizeof(Tx_buf));
                                sprintf(Tx_buf, "[DATA]%d,", iTxDataIndex);
                                memcpy(Tx_buf + iDataPacketHeadSize + 6, sDataMemory[iTxDataIndex - 1], iPacketSize);

                                //나머지 Checksum과 패킷 끝부분 구성
                                Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 0] = ',';
                                Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 1] = checksum;
                                Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 2] = '\r';
                                Tx_buf[iDataPacketHeadSize + 6 + iPacketSize + 3] = '\n';

                                usleep(50000);

                                //현재 전송 상태
                                printf("[Server]Last index = %d\r\n", iDataPacketHeadSize + 6 + iPacketSize + 3);
                                printf("[Server]crc=%2x\r\n", checksum);
                                printf("[Server]Packet=%d\r\n", iTxDataIndex);

                                //보낼 패킷 사이즈 uart thread로 전달
                                iSendPacketSize = iDataPacketHeadSize + 6 + iPacketSize + 3 + 1;

                                //data index counter add
                                iTxDataIndex++;
                            }
                            //부트로더로 부터 업데이트 완료 신호 받으면 완료 여부 표기. 
                            // 현재 MCU 매커니즘 상 재부팅 하고 5초동안 업데이트 할지 여부 확인 하기 때문에
                            //추가적인 동작 방지하기 위해 10초 여유 주고 준비 표시 하도록 설정.  
                            //나중에 config 같은걸 만들어서 버전 확인 해서 완전 자동 구현도 해볼 것
                            else if (fCompareFunction(sTempStringData, "END", 3) == 0)
                            {
                                printf("*****END Update*****\r\n");
                                sleep(15);
                                printf("****READY Update****\r\n");
                            }
                        }
                    }
                    // 수신 데이터 처리했으니 초기화
                    memset(Rx_buf, 0x00, sizeof(Rx_buf));
                }
            }
        }
    }
}

/*
 *  @brief  pThread 함수 생성 및 실행
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

    // UART 송수신 스레드 생성 및 실행
    thr_id = pthread_create(&p_thread[0], NULL, pThread_UART_Function, (void*)p1);
    if (thr_id < 0)
    {
        perror("thread create error : ");
        exit(0);
    }

    pthread_detach(p_thread[0]);
    pthread_join(p_thread[0], NULL);

    // MCU 업데이트 처리 스레드 생성 및 실행
    thr_id = pthread_create(&p_thread[1], NULL, pThread_Update_Comunication, (void*)p2);
    if (thr_id < 0)
    {
        perror("thread create error : ");
        exit(0);
    }

    pthread_detach(p_thread[1]);
    pthread_join(p_thread[1], NULL);
}




