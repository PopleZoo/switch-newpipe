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
  - `X / Y`: OSD 고정 표시 토글

현재 좌우 seek는 비활성화 상태다.

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
- 표시 정보
  - 제목
  - 재생 상태
  - 현재 품질 라벨
  - 진행 바 / 경과 시간 / 총 길이
  - 볼륨 바
- `X / Y`를 누르면 OSD를 잠깐 띄우는 것이 아니라 고정 표시로 토글된다

## 로그

- Switch: `sdmc:/switch/switch_newpipe.log`
- FTP 예시: `ftp://192.168.1.16:5000/switch/switch_newpipe.log`

## 현재 제한

- seek 비활성화
- 화질 수동 선택 UI 없음
- tokenless Android VR UMP는 실기에서 71,936,808 B video와 8,565,660 B audio 전체 완료
- 라이브/장시간 스트림의 예외 처리 강화가 더 필요
- 플레이어 OSD는 들어갔지만 실기 UI 튜닝은 아직 필요하다
