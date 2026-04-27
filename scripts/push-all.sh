#!/bin/bash
# SynthOrbis UNI 推送脚本
# 日常推送：只推 Gitee
# GitHub 里程碑同步：手动执行 git push github master

cd /mnt/d/SynthOrbisUNI

echo "=== SynthOrbis UNI Push ==="
echo "Pushing to Gitee (main repo)..."
timeout 90 git push origin master 2>&1
RET=$?

if [ $RET -eq 0 ]; then
    echo "✅ Gitee push success"
else
    echo "❌ Gitee push failed (exit: $RET)"
fi

echo ""
echo "GitHub sync: 手动执行 'git push github master' 在里程碑完成时"
git rev-list --left-right --count origin/master...github/master 2>/dev/null || echo "(GitHub 未同步)"
