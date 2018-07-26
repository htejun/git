#!/bin/sh

test_description='git describe should show cherry-picks correctly

           C
 o----o----x
      |\ 
      | .--o
      |\  C1
      | .--o
       \  C2
        .--o
          C3

C1 and C3 are cherry-picks from C, and C2 from C1.  Verify git desribe
handles c and its cherry-picks correctly.
'
. ./test-lib.sh

GIT_AUTHOR_EMAIL=bogus_email_address
export GIT_AUTHOR_EMAIL

test_expect_success \
    'prepare repository with topic branches with cherry-picks' \
    'test_tick &&
     echo First > A &&
     git update-index --add A &&
     git commit -m "Add A." &&

     test_tick &&
     git checkout -b T1 master &&
     git checkout -b T2 master &&
     git checkout -b T3 master &&
     git checkout master &&

     test_tick &&
     echo Second > B &&
     git update-index --add B &&
     git commit -m "Add B." &&

     test_tick &&
     git checkout -f T1 &&
     rm -f B &&
     git cherry-pick -x master &&

     test_tick &&
     git checkout -f T2 &&
     rm -f B &&
     git cherry-pick -x T1 &&

     test_tick &&
     git checkout -f T3 &&
     rm -f B &&
     git cherry-pick -x master
'

test_expect_success 'Verify describing cherry-picks' '
     git describe --contains --all --cherry-picks master >actual &&
     echo -e "master\n  T1\n    T2\n  T3" >expect &&
     test_cmp expect actual
'

test_done
