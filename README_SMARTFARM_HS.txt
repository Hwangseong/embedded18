youtube = https://youtu.be/_y3QKy5cOho


���� - �µ��� ������ �����Ͽ�, DB�� �����ϴ� ������ ���� ���α׷�

���� - 4���� Pthread�� ���
       Ư�� �µ� �̻󿡼�, FAN ����
       Ư�� ���� �̻󿡼�, LED ����
       FAN�̳� Ư�� ���ۿ����� �ð����� (ex. 5�ʵ���)

--����--

Pthread1 - �µ��� ���������� ����Ͽ�, �����͸� �����ϰ� shared buff�� put
	   (cond_wait = empty, cond_sinal = fill)
Pthread2 - buff�� ����� �µ������͸� �о�, FAN�� ���۽�ų�� ������ ����
	   (cond_wait = fill, cond_sinal = fill2)
Pthread3 - buff�� ����� ���������͸� �о�, LED�� ���۽�ų�� ������ ����
	   (cond_wait = fill2, cond_sinal = fin)
Pthread4 - get, get1 �Լ��� ���� �� buff�� ����� �����͸� ������, DB�� ����
	   (cond_wait = fin, cond_sinal = empty)

buff2 - �µ������͸� �����ϴ� shared buff
buff3 - ���������͸� �����ϴ� shared buff

int count, seq, fput
count - shared buff�� ���¸� ��Ÿ���� ���º���
seq - �� Pthread�� ���� ������ ��Ÿ���� ���º���
fput - FAN, LED������ ���� ������ ��Ÿ���� ���º���