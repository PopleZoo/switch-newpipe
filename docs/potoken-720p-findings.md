# 720p / PoToken / UMP 조사 결과 (2026-07-17)

이슈 #5의 "약 1분 뒤 멈춤"을 재현하고 PoToken, SABR, UMP, Switch 시스템
브라우저를 차례로 조사했다. 처음에는 PoToken이 필수라고 판단했지만, player client와
전송 방식을 분리해 다시 실험한 결과 더 단순한 경로를 확인했다.

**현재 결론:** HLS가 없는 일반 VOD 720p는 `ANDROID_VR + tokenless direct UMP`로
받을 수 있다. player 요청의 content PoToken, media URL의 `pot`, 쿠키가 모두 없는
상태로 여러 영상의 기존 6~13 MiB 경계를 넘었고 두 영상은 전체 파일을 완주했다.
실제 Switch에서도 71.9 MB video와 8.6 MB audio를 모두 받아 720p 재생에 성공했다.
따라서 Switch에서 BotGuard/WebApplet을 실행할 필요가 없다.

## 1. 재현된 문제

- 일반 `c=ANDROID` adaptive URL의 GET/range 요청은 영상에 따라 약 6~13 MiB 뒤
  HTTP 403이 된다. `bsRH-6Zmsjk`에서는 정확히 6 MiB 다음 요청부터 실패했다.
- iOS HLS가 있으면 720p를 안정적으로 재생할 수 있지만 모든 VOD에 제공되지는 않는다.
- progressive `itag18`은 `ratebypass=yes`라 안정적이지만 최대 360p다.
- 일반 Android와 Android VR은 같은 adaptive itag URL처럼 보여도 CDN 동작이 다르다.

## 2. PoToken/WebApplet 조사에서 확인한 것

호스트에서는 `bgutils-js + jsdom`으로 video-bound content token과 WEB
visitor-bound session token을 만들 수 있었다. 이 토큰을 넣은 Android VR UMP도 전체
파일 다운로드에 성공했다. 그러나 이것만으로 토큰이 필수라는 뜻은 아니었다.

실제 Switch에서는 다음을 확인했다.

- `webPageCreate`와 `webNewsCreate` 모두 loopback URL을 한 번도 요청하지 않고
  "이 기능은 이용하실 수 없습니다"를 표시했다.
- WebApplet reply의 exit reason은 libnx가 이름을 붙이지 않은 값 `10`이었다.
- whitelist가 없는 `WifiWebAuthApplet`도 별도 NIFM 연결 검사에서는
  `0x16a86e` (`2110-2900`)로 종료됐다.
- QuickJS에서는 interpreter와 snapshot까지 실행되지만 작은 DOM shim으로는 WebPo
  minter가 생성되지 않았다. 호스트 A/B 테스트에서는 실제 DOM Element/Event 동작을
  제공하는 `jsdom` 환경만 minter를 반환했다.

Switch에는 일반 사용자용 브라우저는 없고 여러 목적별 browser applet이 있다.
일반 WebApplet은 host application의 whitelist를 사용하고 WifiWebAuthApplet은 captive
portal 전용이다. 자세한 구조는 [Switchbrew Internet Browser 문서](https://switchbrew.org/wiki/Internet_Browser)와
[libnx web API](https://switchbrew.github.io/libnx/web_8h.html)를 참고한다.

이 결과 때문에 WebApplet 기반 구현은 실기에서 신뢰할 수 없었다. 다행히 아래의
tokenless 결과로 브라우저 우회 자체가 불필요해졌다.

## 3. 결정적인 A/B 실험

PoToken을 사용하던 direct UMP PoC에서 다음 두 항목만 제거했다.

1. Android VR player 요청의 `serviceIntegrityDimensions.poToken`
2. video/audio URL의 `pot` query parameter

나머지는 동일하게 유지했다.

- client: `ANDROID_VR` 1.65.10 / Oculus Quest 3
- server-issued WEB `visitorData`
- direct adaptive itag 136/140 URL
- query: `ump=1&srfvp=1&alr=yes&range=...&rn=...`
- POST body: `[120, 0]`
- 1 MiB 단위 range
- UMP type 21 MEDIA의 첫 discriminator byte를 제외한 MP4 payload만 기록
- type 43 redirect 추적, type 58 StreamProtectionStatus 검사

결과는 PoToken을 사용했을 때와 같았다. 즉 이전 성공의 핵심 변수는 PoToken이 아니라
`ANDROID_VR` direct URL과 UMP POST 조합이었다.

## 4. 쿠키·PoToken 없는 호스트 실측

| 영상 | 검증 범위 | 결과 |
|---|---:|---|
| `bsRH-6Zmsjk` | video 17,783,252 B | 전체 완료 |
| `bsRH-6Zmsjk` | audio itag140 2,317,472 B | 전체 완료 |
| `dQw4w9WgXcQ` | video 26,455,880 B | 전체 완료 |
| `ruzSopZIFKc` | video 앞 20 MiB / 전체 71,936,808 B | 20 MiB 경계 통과 |

tokenless `dQw4w9WgXcQ` 결과의 SHA-256은
`569a48fa22b1e0d45ff53c7b61c662b7da148dbe65fe5a22e4fa1cb161508e9f`다.
PoToken direct UMP 결과 및 full SABR 결과와 바이트 단위로 같다.

일반 Android URL은 정상 PoToken을 넣어도 `bsRH-6Zmsjk`에서 6 MiB 뒤 실패했다.
따라서 "토큰 유무"와 "player client 종류"를 섞어 해석하면 안 된다.

## 5. Switch 구현

`표준 720p`의 현재 순서는 다음과 같다.

1. iOS 720p HLS
2. tokenless Android VR direct UMP 720p AVC + AAC
3. progressive `itag18` 360p fallback

구현 구성은 다음과 같다.

- `YouTubeResolver`: WEB visitorData bootstrap 후 tokenless Android VR 720p/AAC URL 선택
- `SwitchPlayer`: video/audio를 각각 1 MiB UMP range POST로 다운로드
- `ump.cpp`: MEDIA unwrap, redirect parameter 병합, StreamProtectionStatus 파싱
- PoToken provider와 WebApplet 호출은 재생 경로와 빌드에서 제거

### 실제 Switch 검증

`192.168.1.11`의 실기에서 HLS가 없는 `ruzSopZIFKc`를 실행해 다음 로그를 확인했다.

- `youtube: selected tokenless Android VR UMP video=ruzSopZIFKc`
- video itag 136 / `clen=71936808`, audio itag 140 / `clen=8565660`
- `PLAYBACK STARTED detail=720P AVC UMP`
- audio 8,565,660 B 전체 prefetch 완료
- video 71,936,808 B 전체 다운로드 완료
- 6, 13, 20 MiB 경계를 모두 통과했고 UMP HTTP/protection 오류나 360p fallback이 없었음

따라서 호스트 PoC뿐 아니라 실제 Switch의 resolver, UMP parser, video/audio 캐시 브리지,
mpv attach를 포함한 전체 경로가 검증됐다.

## 6. 한계

- 이 동작은 YouTube CDN/player 정책에 따라 바뀔 수 있다.
- age restriction, 지역 제한, 유료 콘텐츠처럼 Android VR direct format이 나오지 않는
  영상은 360p fallback 대상이다.
- 라이브/SABR-only 응답과 seek는 이번 구현 범위 밖이다.
- 일반 VOD 실기 경로는 검증했지만 더 다양한 지역/연령 제한 영상은 추가 표본이 필요하다.

## 재현

```bash
make test-ump
./build.sh --app-only
```

첫 명령은 UMP envelope, redirect, protection status, 1 MiB media part를 검증하고,
두 번째 명령은 실제 Switch용 NRO를 빌드한다. 네트워크 실측 결과와 실기 로그의
검증 기준은 위 표와 `docs/testing.md`에 기록했다.
