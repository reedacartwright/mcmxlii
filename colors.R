library(colorspace)
library(RColorBrewer)

wheel <- function(col, radius = 1, ...)
	pie(rep(1, length(col)), col = col, radius = radius, ...)

set1 = as(hex2RGB(brewer.pal(9,"Set1")[-7]), "LAB")

black = as(hex2RGB("#000000"), "LAB")
white = as(hex2RGB("#FFFFFF"), "LAB")

s = c()
for(alpha in rev(seq(0,0.6,length.out=4))) {
	m = mixcolor(alpha, set1, white)
	s = c(s,hex(m,fixup=TRUE))
}
for(alpha in seq(0,0.6,length.out=4)[-1]) {
	m = mixcolor(alpha, set1, black)
	s = c(s,hex(m,fixup=TRUE))
}
mat = matrix(1:length(s),ncol=8, byrow=TRUE)

par(mfrow=c(1,2),mai=c(0.5,0.5,0.5,0.5))

wheel(s)
wheel(s[mat])


d = cbind(coords(hex2RGB(s)),1.0)
d = apply(d, 1, function(x) paste(sprintf("%0.16f",x),collapse=',' ))
d = paste("{",d,"}",sep="",collapse=",\n")
