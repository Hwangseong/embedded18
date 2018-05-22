youtube = https://youtu.be/_y3QKy5cOho


목적 - 온도와 조도를 측정하여, DB에 저장하는 목적을 가진 프로그램

조건 - 4개의 Pthread를 사용
       특정 온도 이상에서, FAN 동작
       특정 조도 이상에서, LED 동작
       FAN이나 특정 동작에서의 시간제약 (ex. 5초동안)

--설명--

Pthread1 - 온도와 조도센서를 사용하여, 데이터를 측정하고 shared buff에 put
	   (cond_wait = empty, cond_sinal = fill)
Pthread2 - buff에 저장된 온도데이터를 읽어, FAN을 동작시킬지 말지를 결정
	   (cond_wait = fill, cond_sinal = fill2)
Pthread3 - buff에 저장된 조도데이터를 읽어, LED를 동작시킬지 말지를 결정
	   (cond_wait = fill2, cond_sinal = fin)
Pthread4 - get, get1 함수를 통해 각 buff에 저장된 데이터를 꺼내고, DB에 저장
	   (cond_wait = fin, cond_sinal = empty)

buff2 - 온도데이터를 저장하는 shared buff
buff3 - 조도데이터를 저장하는 shared buff

int count, seq, fput
count - shared buff의 상태를 나타내는 상태변수
seq - 각 Pthread의 진행 순서를 나타내는 상태변수
fput - FAN, LED까지의 진행 순서를 나타내는 상태변수