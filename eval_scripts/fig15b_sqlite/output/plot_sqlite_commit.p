set terminal postscript eps enhanced color lw 1.5
set lmargin 17
set bmargin 7
set tmargin 5
set key font ", 32"
set xtics font ", 35" offset 0,-0.5
set ytics font ", 35"
set ylabel font "helvetica, 45"
set xlabel font "helvetica, 45"
set xlabel "Workloads" offset 0,-2
set ylabel "Throughput(req/s)" offset -6
set logscale y 10
set ytic auto
set yrange [1:]
set boxwidth 0.8 relative
set style data histograms
set style histogram cluster gap 1 errorbars linewidth 1.5
set style fill solid border -1
set key samplen 2
set key horizontal left tmargin


#set output "sqlite-commit-flexsnap.eps"
#plot 'sqlite_summary' u 2:3:xtic(1) title "SQLite" fillstyle pattern 5 lc rgb "#FF0000",\
#'' u 4:5:xtic(1) title "SQLite-FC" fillstyle pattern 4 lc rgb "#0000FF"

#set output "sqlite-commit-btrfs.eps"
#plot 'sqlite_summary' u 6:7:xtic(1) title "SQLite" fillstyle pattern 5 lc rgb "#FF0000",\
#'' u 8:9:xtic(1) title "SQLite-Btrfs" fillstyle pattern 4 lc 4

set output "sqlite-commit-XFS.eps"
plot 'sqlite_summary' u 2:3:xtic(1) title "SQLite" fillstyle pattern 5 lc rgb "#FF0000",\
'' u 4:5:xtic(1) title "SQLite-XFS" fillstyle pattern 4 lc rgb "#0000FF"

