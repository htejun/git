#!/bin/sh

test_description='Verify xref-cherry-picks handling

Assume the following git repository.

	  D*---E** release-B
	 /
	C*      E* release-D
       /       /
  A---B---C---D---E master

which contains the following cherry-picks.

  C -> C*
  D -> D*
  E -> E* -> E**

1. Build the above repository using `git-cherry-pick -x` with the
   sample post-cherry-pick hook enabled.  Verify that the
   xref-cherry-picks notes are populated correctly.

2. Clear the notes and rebuild them by directly running
   git-reverse-xref-trailers and verify the output.

3. Run it again and check the output still agrees to verify duplicate
   handling.

4. Build a cloned repository using per-branch fetches with the sample
   post-fetch hook enabled. Verify that the xref-cherry-picks notes
   are populatec correctly.
'

TEST_NO_CREATE_REPO=1

. ./test-lib.sh

GIT_AUTHOR_EMAIL=bogus_email_address
export GIT_AUTHOR_EMAIL

test_expect_success \
    'Build repo with cherry-picks and verify xref-cherry-picks' \
    'test_create_repo main &&
     cd main &&
     mkdir -p .git/hooks &&
     mv .git/hooks-disabled/post-cherry-pick.sample .git/hooks/post-cherry-pick &&

     test_tick &&
     echo A >> testfile &&
     git update-index --add testfile &&
     git commit -am "A" &&
     echo B >> testfile &&
     git commit -am "B" &&
     echo C >> testfile &&
     git commit -am "C" &&
     echo D >> testfile &&
     git commit -am "D" &&
     echo E >> testfile &&
     git commit -am "E" &&

     test_tick &&
     git checkout -b release-D master^ &&
     git cherry-pick -x master &&

     test_tick &&
     git checkout -b release-B master^^^ &&
     git cherry-pick -x release-D^^ &&
     git cherry-pick -x release-D^ &&
     git cherry-pick -x release-D &&

     cat > expect <<-EOF &&
master E
Notes (xref-cherry-picks):
    Cherry-picked-to: release-D
    Cherry-picked-to:   release-B

master~1 D
Notes (xref-cherry-picks):
    Cherry-picked-to: release-B~1

master~2 C
Notes (xref-cherry-picks):
    Cherry-picked-to: release-B~2

master~3 B
master~4 A
EOF

     git log --pretty=oneline --notes=xref-cherry-picks master | git name-rev --name-only --stdin > actual &&
     test_cmp expect actual
'

test_expect_success \
    'Clear, rebuild and verify xref-cherry-picks' \
    'git reverse-trailer-xrefs --xref-cherry-picks --all --clear &&
     git reverse-trailer-xrefs --xref-cherry-picks --all &&
     git log --pretty=oneline --notes=xref-cherry-picks master | git name-rev --name-only --stdin > actual &&
    test_cmp expect actual
'

test_expect_success \
    'Build it again to verify duplicate handling' \
    'git reverse-trailer-xrefs --xref-cherry-picks --all &&
     git log --pretty=oneline --notes=xref-cherry-picks master | git name-rev --name-only --stdin > actual &&
    test_cmp expect actual
'

test_expect_success \
    'Build a clone through per-branch fetches and verify xref-cherry-picks' \
    'cd .. &&
     test_create_repo clone &&
     cd clone &&
     mkdir -p .git/hooks &&
     mv .git/hooks-disabled/post-fetch.sample .git/hooks/post-fetch &&

     git fetch -fu ../main master:master &&
     git fetch -fu ../main release-D:release-D &&
     git fetch -fu ../main release-B:release-B &&

     git log --pretty=oneline --notes=xref-cherry-picks master | git name-rev --name-only --stdin > actual &&
    test_cmp ../main/expect actual
'

test_done
