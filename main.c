#include "main.h"

int main(int argc, char** argv)
{
	if (argc == 0)
	{
		printf("App Start Error!!\r\n");
		return -1;
	}
	
	// pThread ���� �� ����
	fPtheradStart();
	
	while (1)
	{
		// pThread �����ϴµ��� Main Loop ����
		sleep(1);
	}
	return 0;
}