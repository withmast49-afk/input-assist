# Input Assist (Geode, GD 2.2081)

Mod hỗ trợ input cho Geometry Dash 2.2081 (Windows 64 + Android 64), chỉ classic.
Hỗ trợ: cube, ship, ufo, wave, ball, robot (spider, swing, platformer, dual thì bỏ qua).

## Build bằng điện thoại (Termux)
1. Giải nén zip vào Termux (ví dụ `~/InputAssist`), `cd InputAssist`.
2. `bash termux-setup.sh` (cài git + gh, đăng nhập GitHub, tạo repo `input-assist`, push).
3. Các lần sau: `bash push.sh "nội dung"`. Script tự chờ Actions build rồi chép `.geode` vào Download.
4. Bỏ file `.geode` vào thư mục `geode/mods` của game.

## Cách hoạt động
Mỗi bước vật lý 240 Hz, mod mô phỏng trước `lookahead-ms` mili giây bằng port vật lý GD 2.2
(`src/sim.hpp`). Nếu input hiện tại chết mà input ngược lại sống thì đổi input.
Nếu quãng đường dự đoán chạm orb, pad, portal, slope... (mod chưa mô phỏng) thì không can thiệp.
