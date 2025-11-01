#!/bin/bash
# Quick feature branch merge script for yaze
# Merges feature → develop → master, pushes, and cleans up

set -e  # Exit on error

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Usage
if [ $# -lt 1 ]; then
    echo -e "${RED}Usage: $0 <feature-branch-name>${NC}"
    echo ""
    echo "Examples:"
    echo "  $0 feature/new-audio-system"
    echo "  $0 fix/memory-leak"
    echo ""
    exit 1
fi

FEATURE_BRANCH="$1"

echo ""
echo -e "${CYAN}═══════════════════════════════════════${NC}"
echo -e "${CYAN}  Quick Feature Merge: ${YELLOW}${FEATURE_BRANCH}${NC}"
echo -e "${CYAN}═══════════════════════════════════════${NC}"
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d ".git" ]; then
    echo -e "${RED}❌ Error: Not in yaze project root!${NC}"
    exit 1
fi

# Save current branch
ORIGINAL_BRANCH=$(git branch --show-current)
echo -e "${BLUE}📍 Current branch: ${CYAN}${ORIGINAL_BRANCH}${NC}"

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo -e "${RED}❌ You have uncommitted changes. Please commit or stash first.${NC}"
    git status --short
    exit 1
fi

# Fetch latest
echo -e "${BLUE}🔄 Fetching latest from origin...${NC}"
git fetch origin

# Check if feature branch exists
if ! git show-ref --verify --quiet "refs/heads/${FEATURE_BRANCH}"; then
    echo -e "${RED}❌ Branch '${FEATURE_BRANCH}' not found locally${NC}"
    exit 1
fi

echo ""
echo -e "${BLUE}📊 Commits in ${YELLOW}${FEATURE_BRANCH}${BLUE} not in develop:${NC}"
git log develop..${FEATURE_BRANCH} --oneline --decorate | head -10
echo ""

# Step 1: Merge into develop
echo -e "${BLUE}[1/5] Merging ${YELLOW}${FEATURE_BRANCH}${BLUE} → ${CYAN}develop${NC}"
git checkout develop
git pull origin develop --ff-only
git merge ${FEATURE_BRANCH} --no-edit

echo -e "${GREEN}✅ Merged into develop${NC}"
echo ""

# Step 2: Merge develop into master
echo -e "${BLUE}[2/5] Merging ${CYAN}develop${BLUE} → ${CYAN}master${NC}"
git checkout master
git pull origin master --ff-only
git merge develop --no-edit

echo -e "${GREEN}✅ Merged into master${NC}"
echo ""

# Step 3: Push master
echo -e "${BLUE}[3/5] Pushing ${CYAN}master${BLUE} to origin...${NC}"
git push origin master

echo -e "${GREEN}✅ Pushed master${NC}"
echo ""

# Step 4: Update and push develop
echo -e "${BLUE}[4/5] Syncing ${CYAN}develop${BLUE} with master...${NC}"
git checkout develop
git merge master --ff-only
git push origin develop

echo -e "${GREEN}✅ Pushed develop${NC}"
echo ""

# Step 5: Delete feature branch
echo -e "${BLUE}[5/5] Cleaning up ${YELLOW}${FEATURE_BRANCH}${NC}"
git branch -d ${FEATURE_BRANCH}

# Delete remote branch if it exists
if git show-ref --verify --quiet "refs/remotes/origin/${FEATURE_BRANCH}"; then
    git push origin --delete ${FEATURE_BRANCH}
    echo -e "${GREEN}✅ Deleted remote branch${NC}"
fi

echo -e "${GREEN}✅ Deleted local branch${NC}"
echo ""

# Return to original branch if it still exists
if [ "$ORIGINAL_BRANCH" != "$FEATURE_BRANCH" ]; then
    if git show-ref --verify --quiet "refs/heads/${ORIGINAL_BRANCH}"; then
        git checkout ${ORIGINAL_BRANCH}
        echo -e "${BLUE}📍 Returned to ${CYAN}${ORIGINAL_BRANCH}${NC}"
    else
        echo -e "${BLUE}📍 Staying on ${CYAN}develop${NC}"
    fi
fi

echo ""
echo -e "${GREEN}╔═══════════════════════════════════════╗${NC}"
echo -e "${GREEN}║          🚀  SUCCESS!  🚀              ║${NC}"
echo -e "${GREEN}║  Feature merged and deployed          ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════╝${NC}"
echo ""
echo -e "${CYAN}What happened:${NC}"
echo -e "  ✅ ${YELLOW}${FEATURE_BRANCH}${NC} → ${CYAN}develop${NC}"
echo -e "  ✅ ${CYAN}develop${NC} → ${CYAN}master${NC}"
echo -e "  ✅ Pushed both branches"
echo -e "  ✅ Deleted ${YELLOW}${FEATURE_BRANCH}${NC}"
echo ""
echo -e "${CYAN}Current state:${NC}"
git log --oneline --graph --decorate -5
echo ""

