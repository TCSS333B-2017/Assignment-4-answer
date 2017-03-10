#include "pixutils.h"
#include "bmp/bmp.h"

//private methods -> make static
static pixMap* pixMap_init();
static pixMap* pixMap_copy(pixMap *p);

//plugin methods
static void rotate(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);
static void convolution(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);
static void flipVertical(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);
static void flipHorizontal(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);

static pixMap* pixMap_init(){
	pixMap *p=malloc(sizeof(pixMap));
	p->pixArray_overlay=0;
	return p;
}	

void pixMap_destroy (pixMap **p){
	pixMap *this_p=*p;
	if(this_p->pixArray_overlay)
	 free(this_p->pixArray_overlay);
	if(this_p->image)free(this_p->image);	
	free(this_p);
	this_p=0;
}
	
pixMap *pixMap_read(char *filename){
	pixMap *p=pixMap_init();
 int error;
 if((error=lodepng_decode32_file(&(p->image), &(p->imageWidth), &(p->imageHeight),filename))){
  fprintf(stderr,"error %u: %s\n", error, lodepng_error_text(error));
  return 0;
	}
	p->pixArray_overlay=malloc(p->imageHeight*sizeof(rgba*));
	p->pixArray_overlay[0]=(rgba*) p->image;
	for(int i=1;i<p->imageHeight;i++){
  p->pixArray_overlay[i]=p->pixArray_overlay[i-1]+p->imageWidth;
	}			
	return p;
}
int pixMap_write(pixMap *p,char *filename){
	int error=0;
 if(lodepng_encode32_file(filename, p->image, p->imageWidth, p->imageHeight)){
  fprintf(stderr,"error %u: %s\n", error, lodepng_error_text(error));
  return 1;
	}
	return 0;
}	 

pixMap *pixMap_copy(pixMap *p){
	pixMap *new=pixMap_init();
	*new=*p;
	new->image=malloc(new->imageHeight*new->imageWidth*sizeof(rgba));
	memcpy(new->image,p->image,p->imageHeight*p->imageWidth*sizeof(rgba));	
	new->pixArray_overlay=malloc(new->imageHeight*sizeof(void*));
	new->pixArray_overlay[0]=(rgba*) new->image;
	for(int i=1;i<new->imageHeight;i++){
  new->pixArray_overlay[i]=new->pixArray_overlay[i-1]+new->imageWidth;
	}	
	return new;
}

	
void pixMap_apply_plugin(pixMap *p,plugin *plug){
	pixMap *copy=pixMap_copy(p);
	for(int i=0;i<p->imageHeight;i++){
	 for(int j=0;j<p->imageWidth;j++){
			plug->function(p,copy,i,j,plug->data);
		}
	}
	pixMap_destroy(&copy);	 
}

int pixMap_write_bmp16(pixMap *p,char *filename){
 BMP16map *bmp16=BMP16map_init(p->imageHeight,p->imageWidth,0,5,6,5); //initialize the bmp type
 if(!bmp16) return 1;
 
 char Bbits=5;
 char Gbits=6;
 char Rbits=5;
 char Abits=0;
 for(int i=0;i<p->imageHeight;i++){
		for(int j=0;j<p->imageWidth;j++){
			const int pix_i=p->imageHeight-i-1;
			const int pix_j=j;
			uint16_t bmp16Pixel=0;
   bmp16Pixel  =  p->pixArray_overlay[pix_i][pix_j].b >> (8-Bbits);
   bmp16Pixel |= (p->pixArray_overlay[pix_i][pix_j].g >> (8-Gbits)) << Bbits;
	  bmp16Pixel |= (p->pixArray_overlay[pix_i][pix_j].r >> (8-Rbits)) << (Bbits+Gbits);
	  bmp16Pixel |= (p->pixArray_overlay[pix_i][pix_j].a >> (8-Abits)) << (Bbits+Gbits+Rbits);
	  bmp16->pixArray[i][j]=bmp16Pixel;
		}			
	}
 //pixMap and BMP16_map are "upside down" relative to each other
 //need to flip one of the the row indices when copying

 BMP16map_write(bmp16,filename);
 BMP16map_destroy(&bmp16);
 return 0;
}	 
void plugin_destroy(plugin **plug){
	if(!plug || !*plug) return;

 if((*plug)->data){
		free ((*plug)->data);
	}
	if(*plug)free(*plug);
	*plug=0;		
}

plugin *plugin_parse(char *argv[] ,int *iptr){
	//malloc new plugin
	plugin *new=malloc(sizeof(plugin));
	new->function=0;
	new->data=0;
	
	int i=*iptr;
	if(!strcmp(argv[i]+2,"rotate")){
		new->function=&(rotate);
		new->data=malloc(2*sizeof(float));
		float* sc=(float*) new->data;
		float theta=atof(argv[i+1]);
	 sc[0]=sin(degreesToRadians(-theta));
	 sc[1]=cos(degreesToRadians(-theta)); 
	 *iptr=i+2;
  return new;
	}	
	if(!strcmp(argv[i]+2,"convolution")){
		new->function=&(convolution);
		new->data=malloc(9*sizeof(int)); //allocate room for kernel
		int (*array)[3] =new-> data;     //cast it to a type int[3] pointer i.e. the type of pointer that array of int[3] i.e. array[3][3] decays to 
		int j=0;
  for(int m=0;m<3;m++){
   for(int n=0;n<3;n++){	
				array[m][n]=atoi(argv[i+j+1]);	//careful with indices - want to start with argv[i+1] and to use atoi
				j++;
			}
		}
		*iptr=i+10;	
  return new;
	}
	if(!strcmp(argv[i]+2,"flipHorizontal")){
		new->function=&(flipHorizontal);
  *iptr=i+1;
  return new;
	}
	if(!strcmp(argv[i]+2,"flipVertical")){
		new->function=&(flipVertical);
  *iptr=i+1;
  return new;
	}
	free(new);		//in case of error should free the memory allocated to new before returning
	return(0);
}	

//define plugin functions

static void rotate(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
	//void data is s and c (sin and cos from previous function)
	const float ox=p->imageWidth/2.0f;
 const float oy=p->imageHeight/2.0f;
 float *sc=(float*) data;
 const float s=sc[0];
	const float c=sc[1];
	
 float rotx = c*(j-ox) - s * (oy-i) + ox;
 float roty = -(s*(j-ox) + c * (oy-i) - oy);
	int rotj=rotx+.5;
	int roti=roty+.5; 
	if(roti >=0 && roti < oldPixMap->imageHeight && rotj >=0 && rotj < oldPixMap->imageWidth){
  memcpy(p->pixArray_overlay[i]+j,oldPixMap->pixArray_overlay[roti]+rotj,sizeof(rgba));			 
	}
	else{
		memset(p->pixArray_overlay[i]+j,0,sizeof(rgba));			
	}	
}

static void convolution(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
 int (*weights)[3]= data;

 int maxRow=p->imageHeight-1; //max value for row
 int maxCol=p->imageWidth-1;  //max value for width
 int maxRGBValue=255;
 int m,n;
 int rsum=0;  //accumulators
 int gsum=0;
 int bsum=0;
 int norm=0;
 for (m=-1;m<=1;m++){  //I think in this case <= is clearer but m<2 is fine
		int jmn=j+m;
		if(jmn <0) jmn=0;
		if(jmn > maxCol) jmn=maxCol;
  for (n=-1;n<=1;n++){
			int imn=i+n;
			if(imn <0) imn=0;
			if(imn > maxRow)imn=maxRow;
			rsum+=oldPixMap->pixArray_overlay[imn][jmn].r*weights[m+1][n+1];
			gsum+=oldPixMap->pixArray_overlay[imn][jmn].g*weights[m+1][n+1];
			bsum+=oldPixMap->pixArray_overlay[imn][jmn].b*weights[m+1][n+1];
			norm+=weights[m+1][n+1]; //our indices started at -1 but the kernel indices start at 0 so we add 1 to indices
			
			//can use 1-d array instead for 2-d kernel for weights
			//set k to zero outside loop
			//multiply by myWeight=weights[k++];
			//
		}	
	}
	//normalize if norm is not zero
	if(norm){
	 rsum/=norm;
	 bsum/=norm;
	 gsum/=norm;
	}
	//check for underflow/overflow i.e. that the final values are between 0-255
 if(rsum <0) rsum=0;
 if(rsum > maxRGBValue) rsum = maxRGBValue;
 if(gsum <0) gsum=0;
 if(gsum > maxRGBValue) gsum = maxRGBValue;
 if(bsum <0) bsum=0;
 if(bsum > maxRGBValue) bsum = maxRGBValue;
 
 //the normed accumulators are the new pixel rgb values
 p->pixArray_overlay[i][j].r=rsum;
 p->pixArray_overlay[i][j].g=gsum;
 p->pixArray_overlay[i][j].b=bsum; 
}

 
static void flipVertical(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
 //reverse the pixels vertically - can be done in one line		
 memcpy(p->pixArray_overlay[i]+j,oldPixMap->pixArray_overlay[oldPixMap->imageHeight-1-i]+j,sizeof(rgba));
}	 
static void flipHorizontal(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
 //reverse the pixels horizontally - can be done in one line
  memcpy(p->pixArray_overlay[i]+j,oldPixMap->pixArray_overlay[i]+oldPixMap->imageWidth-1-j,sizeof(rgba));
}
