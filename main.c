#include "main.h"

int main(int argc, char** argv)
{
	if (argc == 0)
	{
		printf("App Start Error!!\r\n");
		return -1;
	}
	
	// pThread 생성 및 실행
	fPtheradStart();
	
	while (1)
	{
		// pThread 동작하는동안 Main Loop 동작
		sleep(1);
	}
	return 0;
}