#!/data/data/com.termux/files/usr/bin/bash
# Chạy 1 lần trong thư mục InputAssist: bash termux-setup.sh
set -e
termux-setup-storage || true
pkg update -y
pkg install -y git gh

# trust folder này (tránh lỗi "dubious ownership")
git config --global --add safe.directory "$(pwd)"
git config --global init.defaultBranch main
if [ -z "$(git config --global user.name)" ]; then read -r -p "Tên git: " n; git config --global user.name "$n"; fi
if [ -z "$(git config --global user.email)" ]; then read -r -p "Email git: " e; git config --global user.email "$e"; fi

# scope workflow bắt buộc để push thư mục .github/workflows
if ! gh auth status >/dev/null 2>&1; then
  gh auth login -h github.com -p https -s workflow,repo
fi
gh auth setup-git

[ -d .git ] || git init
git add -A
git commit -m "Input Assist v0.2.0" || true

if gh repo view input-assist >/dev/null 2>&1; then
  git remote get-url origin >/dev/null 2>&1 || git remote add origin "$(gh repo view input-assist --json url -q .url)"
  git push -u origin main
else
  gh repo create input-assist --public --source=. --remote=origin --push
fi
echo "Xong. Từ giờ dùng: bash push.sh \"nội dung commit\""
