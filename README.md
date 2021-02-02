# CNetServer
* 외부통신을 위한 서버 네트워크 라이브러리
* 외부통신 환경이므로 Encode, Decode와 같은 암호화과정이 포함

**사용방법**
1. 자신만의 MyClass를 선언한 후, CNetServer를 상속받습니다.
2. 상속받은 클래스에서 필수 핸드러들을 오버라이딩합니다.
      - OnConnectionRequest(WCHAR* clientIP, SHORT clientPort); (accept호출 직후)
      - OnClientJoin(UINT64 sessionID); (세션셋팅완료 직후)
      - OnRecv(UINT64 sessionID, CSerializationBuffer* pPacket); (원격지에서 데이터가 도착했을 때)
      - OnClientLeave(UINT64 sessionID); (세션이 종료되었을 때)
      
    상황에 따라 필요 시 다음과 같은 핸들러들을 추가로 오버라이딩 할 수 있습니다.
      
      - OnSend(UINT64 sessionID); (패킷 송신 완료 후)
      - OnWorkerThreadBegin(UINT64 sessionID, DWORD transferredBytes, BYTE abortedFlag); (워커스레드 루프 시작 시)
      - OnWorkerThreadEnd(); (워커스레드 루프 종료 시)

3. 상속된 객체를 통해 Start()함수를 호출합니다. 이 함수로 IP, 포트, 워커스레드 동작 갯수, 워커스레드 생성 갯수, 네이글 옵션 여부, 최대 클라이언트 수 를 정할 수 있습니다.
4. 서버가동을 멈출때는 Stop()함수를 호출합니다. 이 함수를 호출하고 나서 몇 초 뒤 종료됩니다.
