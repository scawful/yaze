#!/bin/bash
# Script to create a proper release tag for YAZE
# Usage: ./scripts/create_release.sh [version]
# Example: ./scripts/create_release.sh 0.3.0

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

# Check if we're in a git repository
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    print_error "Not in a git repository!"
    exit 1
fi

# Check if we're on master branch
current_branch=$(git branch --show-current)
if [ "$current_branch" != "master" ]; then
    print_warning "You're on branch '$current_branch', not 'master'"
    read -p "Continue anyway? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Switching to master branch..."
        git checkout master
    fi
fi

# Get version argument or prompt for it
if [ $# -eq 0 ]; then
    echo
    print_info "Enter the version number (e.g., 0.3.0, 1.0.0-beta, 2.1.0-rc1):"
    read -p "Version: " version
else
    version=$1
fi

# Validate version format
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-.*)?$ ]]; then
    print_error "Invalid version format: '$version'"
    print_info "Version must follow semantic versioning (e.g., 1.2.3 or 1.2.3-beta)"
    exit 1
fi

# Create tag with v prefix
tag="v$version"

# Check if tag already exists
if git tag -l | grep -q "^$tag$"; then
    print_error "Tag '$tag' already exists!"
    exit 1
fi

# Show current status
echo
print_info "Current repository status:"
git status --short

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    print_warning "You have uncommitted changes!"
    read -p "Continue with creating release? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Please commit your changes first, then run this script again."
        exit 1
    fi
fi

# Show what will happen
echo
print_info "Creating release for YAZE:"
echo "  📦 Version: $version"
echo "  🏷️  Tag: $tag"
echo "  🌿 Branch: $current_branch"
echo "  📝 Changelog: docs/C1-changelog.md"
echo

# Confirm
read -p "Create this release? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    print_info "Release creation cancelled."
    exit 0
fi

# Create and push tag
print_info "Creating tag '$tag'..."
if git tag -a "$tag" -m "Release $tag"; then
    print_success "Tag '$tag' created successfully!"
else
    print_error "Failed to create tag!"
    exit 1
fi

print_info "Pushing tag to origin..."
if git push origin "$tag"; then
    print_success "Tag pushed successfully!"
else
    print_error "Failed to push tag!"
    print_info "You can manually push with: git push origin $tag"
    exit 1
fi

echo
print_success "🎉 Release $tag created successfully!"
print_info "The GitHub Actions release workflow will now:"
echo "  • Build packages for Windows, macOS, and Linux"
echo "  • Extract changelog from docs/C1-changelog.md"
echo "  • Create GitHub release with all assets"
echo "  • Include themes, fonts, layouts, and documentation"
echo
print_info "Check the release progress at:"
echo "  https://github.com/scawful/yaze/actions"
echo
print_info "The release will be available at:"
echo "  https://github.com/scawful/yaze/releases/tag/$tag"
