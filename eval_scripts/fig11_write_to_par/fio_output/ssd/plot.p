set terminal postscript eps enhanced color lw 1.5
#set term postscript eps enhanced monochrome 20 dashed dashlength 1 lw 1.5
#set size ratio 0.75
set lmargin 17
set bmargin 7
set tmargin 5
set key font ", 26"
set xtics font ", 30" offset 0,-0.5
set ytics font ", 32"
set ylabel font "helvetica, 43"
set xlabel font "helvetica, 45"
set xlabel "Num Children" offset 0,-2
set ylabel "Throughput(MiB/s)" offset -5
set logscale y 10
set ytic auto
#set yrange [0:]
#set ytic 50
set boxwidth 0.8 relative
set style data histograms
set style histogram cluster gap 1 errorbars linewidth 1.5
set style fill solid border -1
set key spacing 1
set key samplen 2.5
set key horizontal tmargin center
set key at graph 0.35,1.1
set offset graph -0.1 , -0.15

#set ylabel ""
#set lmargin 12

set output "plot_write_to_par.eps"
plot 'ssd_coldCache_4096bs' u 2:3:xtic(1) title "Btrfs" fillstyle pattern 5 lc rgb "#FF0000", \
'' u 4:5:xtic(1) title "XFS" fillstyle pattern 4 lc rgb "#228B22", \
'' u 6:7:xtic(1) title "Ext4-FC-DP" fillstyle pattern 6 lc rgb "#FFA500"
