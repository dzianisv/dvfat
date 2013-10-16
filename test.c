#include "libdfat.h"
#include <stdio.h>


int main(int argc, char** argv)
{
	dfat_load("/home/denis/projects/СПОВМ/7/dvFAT/image");
	
	struct list l;
	l.count = 0;
	
	return 0;
}