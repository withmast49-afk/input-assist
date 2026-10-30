#!/data/data/com.termux/files/usr/bin/bash
# Commit, push, chờ GitHub Actions build, tải file .geode về thư mục Download.
git config --global --add safe.directory "$(pwd)"
git add -A
git commit -m "${1:-update}" || true
git push || exit 1
sleep 10
RUN=$(gh run list --limit 1 --json databaseId -q '.[0].databaseId')
if gh run watch "$RUN" --exit-status; then
  rm -rf out
  gh run download "$RUN" -D out
  mkdir -p ~/storage/downloads
  find out -name '*.geode' -exec cp {} ~/storage/downloads/ \;
  echo "Đã chép .geode vào thư mục Download."
else
  echo "=== BUILD LỖI, 80 dòng cuối của log (gửi cho Claude) ==="
  gh run view "$RUN" --log-failed | tail -80
  exit 1
fi
