# Playback

## 현재 상태

재생 코드는 이미 연결되어 있다. 현재 앱은 Borealis UI에서 재생 요청을 큐에 넣고, UI를 빠져나온 뒤 Switch 전용 SDL2/mpv 플레이어를 실행한다.

## 현재 재생 구조

1. 목록에서 `A`로 영상 선택
2. `PlaybackRequest` 생성
3. Borealis 종료
4. `SwitchPlayer` 실행
5. YouTube URL이면 `YouTubeResolver`로 playable stream 해석
6. mpv 재생
7. `B`로 종료 후 Borealis 재시작

## 입력

- 목록 화면
  - `A`: 바로 재생
  - `Y`: 상세 정보
- 플레이어 화면
  - `A`: 일시정지 / 재개
  - `B`: 종료
  - `위 / 아래`: 볼륨
  - `좌 / 우`: 10초 이동
  - `LB / RB`: 60초 이동
  - `X / Y`: OSD 고정 표시 토글

## 현재 해석 전략

### 1. 일반 direct URL

- 바로 재생
- 필요 시 캐시 브리지 사용

### 2. YouTube URL

- Android `youtubei/v1/player`로 기본 playable format 조회
- 720p adaptive가 있으면 iOS HLS manifest 확보 시도
- 현재 최신 코드 기준 우선순위
  1. 원본 720p HLS manifest direct 재생 (throttle 면역, mpv가 세그먼트 단위 fetch)
  2. HLS가 없으면 tokenless Android VR 720p AVC/AAC direct URL을 UMP로 재생
  3. Android VR/UMP가 실패하면 progressive `itag18`(`ratebypass=yes`, 360p)
  - `hls-bitrate=max` 적용
  - 첫 프레임 전 실패 시 fallback stream으로 자동 전환

> **YouTube 스트림 차단 주의 (2026-06 이후):** `ratebypass=yes`가 없는
> `c=ANDROID` googlevideo `videoplayback` URL(=720p adaptive)은 초기 버스트
> (~6-13MB, itag136 기준 약 1분 분량)만 주고 그 뒤 **모든 요청을 HTTP 403으로
> 하드 차단**한다. 그래서 단일 GET이든 `&range=` 청크든 버스트 이후를 못 받고
> **~1:04에서 멈춘다** (issue #5). 현재는 일반 GET 대신 tokenless `ANDROID_VR`
> URL을 UMP POST로 1MiB씩 받고 envelope의 MEDIA만 캐시에 써서 이 제한을 우회한다.

### 3. Tokenless Android VR UMP adaptive stream

- `/sw.js_data`의 서버 발급 WEB visitorData를 Android VR player 요청에 사용한다.
- player 요청에 `serviceIntegrityDimensions.poToken`을 넣지 않고, video/audio URL에도
  `pot` query parameter를 추가하지 않는다.
- 영상과 오디오를 각각 1MiB UMP range POST로 받고 type 21 MEDIA를 캐시에 기록한다.
- UMP redirect와 StreamProtectionStatus를 검사한다.
- WebApplet을 사용하지 않으므로 브라우저 제한이나 title whitelist의 영향을 받지 않는다.

### 4. Progressive stream

- `switchcache://` 커스텀 프로토콜 사용
- 백그라운드 다운로드 + mpv 읽기 브리지
- 긴 영상도 전체 다운로드 완료까지 기다리지 않고 재생 시작 가능
- `ratebypass=yes`가 있으면 단일 GET으로 직접 다운로드 (면역, 끝까지 재생됨)
- `ratebypass`가 없는 일반 Android googlevideo GET은 신뢰하지 않는다. 720p는 위 UMP
  경로를 사용하고, 실패하면 `ratebypass=yes` progressive로 전환한다.

## 로딩 UI

재생 준비 중에는 검은 화면 대신 progress circle과 상태 문구를 표시한다.

예시 상태:

- `RESOLVING YOUTUBE STREAM`
- `REQUESTING 720P HLS STREAM`
- `REQUESTING 720P UMP STREAM`
- `OPENING MEDIA STREAM`
- `DOWNLOADING VIDEO DATA`
- `BUFFERING FIRST FRAME`

## 재생 OSD

재생이 시작된 뒤에는 입력 반응형 OSD를 영상 위에 직접 그린다.

- 자동 표시 시점
  - 첫 재생 시작 직후
  - `A` pause / resume
  - `위 / 아래` 볼륨 변경
  - `좌 / 우`, `LB / RB` 시간 이동
- 표시 정보
  - 제목
  - 재생 상태
  - 현재 품질 라벨
  - 진행 바 / 경과 시간 / 총 길이
  - 진행 바 위의 버퍼 구간 (지금 이동 가능한 범위)
  - 볼륨 바
- `X / Y`를 누르면 OSD를 잠깐 띄우는 것이 아니라 고정 표시로 토글된다

## 시간 이동 (seek)

`좌 / 우` 10초, `LB / RB` 60초다. 경로마다 이동 가능한 범위가 다르다.

### HLS / manifest 경로

mpv가 세그먼트를 직접 가져오므로 전체 구간 이동이 된다. 브리지를 쓰지 않으니
별도 제한도 걸지 않는다.

### 스트림 브리지 경로 (progressive / UMP)

브리지는 다운로드를 캐시 파일에 순차적으로 쌓는다. `switchcache` stream callback의
`seek_fn`은 **이미 받아둔 바이트 안에서만** 성공하고, 그 밖은 거절한다. 다운로더가
앞으로만 진행하기 때문에, 아직 안 받은 위치를 기다리면 남은 다운로드가 끝날 때까지
demuxer가 멈춰버린다.

- `clen`으로 총 크기를 알 때는 `받은 바이트 / 총 바이트`를 시간으로 환산해서
  버퍼 끝 2초 앞까지만 이동을 허용한다. 가변 비트레이트에서는 근사치다.
- 분리된 오디오 트랙이 있으면 video / audio 중 더 느린 쪽 비율을 쓴다. 이동 지점이
  두 캐시 안에 모두 들어와야 한다.
- 앞으로 이동이 버퍼 끝에 막히면 이동하지 않고 OSD에 `BUFFERED TO mm:ss`를 띄운다.
- 라이브는 이동하지 않는다.

`size_fn`은 일부러 null로 둔다. byte offset 이동에는 총 크기가 필요 없고, 크기를
알려주면 fragmented MP4에서 ffmpeg `mov_read_mfra()`가 fragment index를 찾으려고
파일 끝으로 이동한다. 부분만 받은 캐시 파일에서 절대 줄 수 없는 offset이 바로 그
파일 끝이다.

> **회귀 시 확인 순서:** 재생 자체가 깨지면 `stream_open`의 `seek_fn`을 다시
> `nullptr`로 돌려 seek만 끄면 이전 동작으로 복귀한다. 720p UMP 경로가 실기에서
> 검증된 경로라 이 순서를 지킬 것.

## 로그

- Switch: `sdmc:/switch/switch_newpipe.log`
- FTP 예시: `ftp://192.168.1.16:5000/switch/switch_newpipe.log`

## 현재 제한

- 브리지 경로의 seek은 이미 받아둔 구간 안에서만 된다
- 화질 수동 선택 UI 없음
- tokenless Android VR UMP는 실기에서 71,936,808 B video와 8,565,660 B audio 전체 완료
- 라이브/장시간 스트림의 예외 처리 강화가 더 필요
- 플레이어 OSD는 들어갔지만 실기 UI 튜닝은 아직 필요하다
