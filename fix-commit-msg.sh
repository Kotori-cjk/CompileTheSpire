#!/bin/bash
# Remove Co-Authored-By line from the release script commit
cd "$(dirname "$0")"

echo "=== Before ==="
git log --format='%h %s' adf1cb7~1..adf1cb7
echo ""

# Rewrite the commit message
git filter-branch -f --msg-filter '
  if [ "$GIT_COMMIT" = "adf1cb75fe168f9b5737755eacedf600fd7f729c" ]; then
    sed "/^Co-Authored-By: Claude/d"
  else
    cat
  fi' adf1cb7~1..HEAD

echo ""
echo "=== After ==="
git log --format='%h %B' adf1cb7~1..HEAD | head -20
echo ""
echo "Done! If the commit message looks clean, force-push with:"
echo "  git push origin feat/gui --force-with-lease"
echo ""
echo "Then switch back to main:"
echo "  git checkout main"
