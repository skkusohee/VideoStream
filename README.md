# VideoStream





### Simulation


#### 실험 세팅

##### 1. Application

동영상 길이는 총 300 frame
1초당 5 frame 영상을 송출한다고 가정
사용자가 특정 배속으로 동영상을 요청하면 1초당 frame 수가 배속에 비례하여 증가

1 frame당 해상도(resolution) level

Resolution Level 0 : 100001 bytes for 1 frame
Resolution Level 1 = 150001 bytes for 1 frame
Resolution Level 2 = 200001 bytes for 1 frame
Resolution Level 3 = 230001 bytes for 1 frame
Resolution Level 4 = 250001 bytes for 1 frame
Resolution Level 5 = 300001 bytes for 1 frame


초기에 resolution level 5 로 동영상을 전송
버퍼링이 1초 이상 지속되면 resolution level을 조절해 버퍼링을 1초 이하로 줄임.
버퍼에 충분한 frame이 존재하면 다시 resolution level을 증가.


##### 2. Topology

1. 1:1 P2P Link
2. 1:1 WiFi Link

