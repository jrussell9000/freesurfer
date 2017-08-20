#! /bin/tcsh -f 

set f = $1

# see whether there are any cerebral WM labels in this file 
# labels of interest: 2, 41

set fname = ${f:r:r}.count.txt
#set cmd = (mri_binarize --match 2 41 --count $fname --i $f)
set cmd = (/autofs/space/turan_001/users/lzollei/dev/mri_binarize/mri_binarize --match 2 41 --count $fname --i $f --noverbose)
eval $cmd
#more $fname
set nums = `cat $fname`
rm -f $fname

# echo NUMS $nums
if ($nums[1] > 0) then
  # exit 1
  echo 1
else
  echo 0
  # exit 0 
endif

exit 0
