#!/bin/bash

# exit codes
success=0
release_failed=1
rollback_failed=2
release_preconditions_failed=4

#The actual exit code
exit_code=$success

# color definitions
KRST="\033[0m"
KBLD="\033[1m"
KFNT="\033[2m"
KNRM="\033[22m"
KRED="\033[31m"
KGRN="\033[32m"
KYEL="\033[33m"
KBLU="\033[34m"
KMAG="\033[35m"
KCYN="\033[36m"
KWHT="\033[37m"
C_ERR="$KRED"
C_INF="$KCYN"
C_QRY="$KYEL"

# file to store release info
# used after interruption and for rollback
release_info="release_info"

# suffix for development snapshots
version_snapshot_suffix="-SNAPSHOT"

# Increment the last part of a simple semver version string (X.Y.Z).
increment_patch() {
  old="$1"
  IFS='.'                     # Set delimiter to .
  set -- $old                 # Split input into $1 $2 $3
  printf "%s.%s.%d\n" "$1" "$2" "$(($3 + 1))"  # Increment last part

}


# calculate proposal for next version number
calculate_next_version() {
 case "$1" in
   *$version_snapshot_suffix)
     # Cut off -SNAPSHOT suffix
     result=$(echo "$1" | sed -e "s/$version_snapshot_suffix$//")
     ;;
   *)
     # Search for the last version part and increase by 1
     # taken from https://stackoverflow.com/q/8653126/572645
     result=$(increment_patch "$1")
     ;;
 esac

 # if a second parameter is given, use this as the next snapshot suffix
 if [ ! -z "$2" ]; then
   echo $result"$2"
 else
   echo $result
 fi
}

# Upload release to github
gh_upload_release() {
  for asset in dist/*; do
    assets="$assets"" -a ""$asset"
  done
  hub release create $assets $new_version_tag
}

# Delete release from github
gh_delete_release() {
  hub release delete $new_version_tag
}

# Perform a rollback
rollback() {
  if [ -z $prev_version_commit_hash ]; then
    printf "$C_ERR""Missing release info. Rollback not possible.""$KRST""\n"
    exit_code=$(echo $exit_code + $rollback_failed | bc)
    exit $exit_code
  fi

  #TODO check that no additional commit was made since release?

  printf "$C_INF""Rollback to""$KRST""\n  $(git log --oneline --no-decorate $prev_version_commit_hash^..$prev_version_commit_hash)\n""$C_INF""?\n""$KRST"
  printf "$C_INF""The following commits will be dropped:\n""$KRST"
  git log --oneline --no-decorate $prev_version_commit_hash..HEAD | sed "s/^/\ \ /"
  read -p $(printf "$C_QRY")"Perform rollback? ("$(printf "$KBLD")"y/n"$(printf "$KNRM")"): "$(printf "$KRST") yn
  case $yn in
    [yY] )
      #gh_delete_release $new_version_tag
      git tag -d $new_version_tag
      git reset --hard $prev_version_commit_hash
      rm $release_info
      printf "$C_INF""Changes were rolled back locally. Please double check them and push them to remote via\n  git push --force-with-lease""$KRST"
      ;;
    * )
      printf "$C_INF""Rollback aborted""$KRST"
      ;;
  esac
  exit $exit_code
}

# Collect all necessary information for a release
prepare_release() {
  touch $release_info

  # remember current state in release_info
	if [ -z $prev_version_commit_hash ]; then
		prev_version_commit_hash=$(git rev-parse HEAD)
    sed -i '/prev_version_commit_hash=/d' $release_info
		echo "prev_version_commit_hash=$prev_version_commit_hash" >> $release_info
	fi

	if [ -z $prev_version ]; then
    prev_version=$(sed -nE 's/^\#define VERSION "([0-9.]+(-SNAPSHOT)?)".*/\1/p' src/version.h)
    sed -i '/prev_version=/d' $release_info
		echo "prev_version=$prev_version" >> $release_info
	fi

  # now request info from user
	if [ -z $new_version ]; then
    proposed_version=$(calculate_next_version $prev_version)
		read -p $(printf "$C_QRY")"Current version is $prev_version. Please enter the new release version (default: "$(printf "$KBLD")"$proposed_version"$(printf "$KNRM")"): "$(printf "$KRST") user_input
		if [ ! -z $user_input ]; then
      new_version=$user_input
    else
      new_version=$proposed_version
    fi
    sed -i '/new_version=/d' $release_info
    echo "new_version=$new_version" >> $release_info
  fi

	if [ -z $new_version_tag ]; then
		proposed_tag=v$new_version
		read -p $(printf "$C_QRY")"Please enter the VCS tag for the new release (default: "$(printf "$KBLD")"$proposed_tag"$(printf "$KNRM")"): "$(printf "$KRST") user_input
		if [ ! -z $user_input ]; then
      new_version_tag=$user_input
    else
      new_version_tag=$proposed_tag
    fi
    sed -i '/new_version_tag=/d' $release_info
    echo "new_version_tag=$new_version_tag" >> $release_info
  fi

	if [ -z $next_version ]; then
		proposed_version=$(calculate_next_version $new_version $version_snapshot_suffix)
		read -p $(printf "$C_QRY")"Please enter the next development version (default: "$(printf "$KBLD")"$proposed_version"$(printf "$KNRM")"): "$(printf "$KRST") user_input

		if [ ! -z $user_input ]; then
      next_version=$user_input
    else
      next_version=$proposed_version
    fi
    sed -i '/next_version=/d' $release_info
    echo "next_version=$next_version" >> $release_info
  fi
}

# Perform the release
perform_release() {
  echo $prev_version_commit_hash

  {
    sed -i "s/VERSION .*/VERSION \"${new_version}\"/" src/version.h && \
    git commit -m "[release] Create new release" src/version.h && \
    git tag -a $new_version_tag && \
    make clean all && \
    sed -i "s/VERSION .*/VERSION \"${next_version}\"/" src/version.h && \
    git commit -m "[release] Prepare next development version" src/version.h && \
    git push && \
    git push origin $new_version_tag && \
    rm release_info
  } || {
    printf "$C_ERR""Error performing release.\n""$KRST"
    exit_code=$(echo $exit_code + $release_failed | bc)
    rollback
  }
}

# Check that the repository is in a clean state
check_clean_state() {
  # check that no uncommited changes exist
  if [ -n "$(git status --porcelain)" ]; then
    # Uncommitted changes
    printf "$C_ERR""You have uncommited changes. Commit them or stash them away before attempting to perform a release.""$KRST"
    exit_code=$(echo $exit_code + $release_preconditions_failed | bc)
    exit $exit_code
  fi

  # check that no remote changes exist that need to be pulled
  # Taken from https://stackoverflow.com/a/3278427/572645
  git remote update
  UPSTREAM=${1:-'@{u}'}
  LOCAL=$(git rev-parse @)
  REMOTE=$(git rev-parse "$UPSTREAM")
  BASE=$(git merge-base @ "$UPSTREAM")

  if [ $LOCAL = $REMOTE ]; then
    # "Up-to-date"
    :
  elif [ $LOCAL = $BASE ]; then
    echo "$C_ERR""There are remote changes. Pull and merge them before attempting to perform a release.""$KRST"
    exit_code=$(echo $exit_code + $release_preconditions_failed | bc)
    exit $exit_code
  elif [ $REMOTE = $BASE ]; then
    # "Need to push"
    :
  else
    printf "$C_ERR""Your local and your remote branch have diverged. Please rectify the situation before attempting to perform a release.""$KRST"
    exit_code=$(echo $exit_code + $release_preconditions_failed | bc)
    exit $exit_code
  fi
}

# Cleanup release info
clean() {
  rm -f release_info
}

# Display usage help
show_help() {
  printf "$C_INF"
  printf "Usage: $0 [perform|rollback|clean]\n"
  printf "Options:\n"
  printf "  perform: Perform a release\n"
  printf "  rollback: Rollback a release\n"
  printf "  clean: Cleanup release info (No rollback will be possible afterwards)\n"
  printf "$KRST"
}

# read existing release info
if [ -f "$release_info" ]; then
  . ./$release_info
fi

# evaluate commandline arguments
case "$1" in
 perform)
   check_clean_state && prepare_release && perform_release
   ;;
 rollback)
   rollback
   ;;
 clean)
   clean
   ;;
 *)
   show_help
   ;;
esac
