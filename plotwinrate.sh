

#num=`grep "^OTGChouFleur.ApplyAction[0-3]$" clientlogs | wc -l `
num=`grep resigned.at clientlogs | wc -l`
echo $num games
skip=$(( 1 + ( ($num - 10 ) / 15 ) ))
for n in `seq 10 $skip $num`
do
#echo $n games
w=`grep resigned.at clientlogs | head -n $n |tail -n $skip | grep black| wc -l`
l=`grep resigned.at clientlogs | head -n $n | tail -n $skip |grep white |wc -l`
#w=`grep "^OTGChouFleur.ApplyAction[0-3]$" clientlogs | head -n $n | grep "[01]$" | wc -l`
#l=`grep "^OTGChouFleur.ApplyAction[0-3]$" clientlogs | head -n $n | grep "[23]$" | wc -l `
wr=$(( 100 * $w / ( $w + $l ) ))
echo -n "$n -->         ----      "
for i in `seq $wr`
do
  echo -n "*"
done
echo " $wr"
done
echo -n "Black: "
grep -c "resigned.at.*black" clientlogs
echo -n "White: "
grep -c "resigned.at.*white" clientlogs
