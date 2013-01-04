
__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;

inline uchar8 getIntensityVector(int i, int j, image2d_t img1, image2d_t img2, image2d_t img3, image2d_t img4, image2d_t img5, image2d_t img6, image2d_t img7, image2d_t img8) {
    
    uchar8 I;
    I.s0 = read_imageui(img1, sampler, (int2)(j,i)).x;
    I.s1 = read_imageui(img2, sampler, (int2)(j,i)).x;
    I.s2 = read_imageui(img3, sampler, (int2)(j,i)).x;
    I.s3 = read_imageui(img4, sampler, (int2)(j,i)).x;
    I.s4 = read_imageui(img5, sampler, (int2)(j,i)).x;
    I.s5 = read_imageui(img6, sampler, (int2)(j,i)).x;
    I.s6 = read_imageui(img7, sampler, (int2)(j,i)).x;
    I.s7 = read_imageui(img8, sampler, (int2)(j,i)).x;
    return I;
}

inline float4 getNormalVector(__global float *Sinv, uchar8 I) {
    
    float4 n;
    n.x =   (Sinv[8*0+0]*I.s0)+
            (Sinv[8*0+1]*I.s1)+
            (Sinv[8*0+2]*I.s2)+
            (Sinv[8*0+3]*I.s3)+
            (Sinv[8*0+4]*I.s4)+
            (Sinv[8*0+5]*I.s5)+
            (Sinv[8*0+6]*I.s6)+
            (Sinv[8*0+7]*I.s7);
    
    n.y =   (Sinv[8*1+0]*I.s0)+
            (Sinv[8*1+1]*I.s1)+
            (Sinv[8*1+2]*I.s2)+
            (Sinv[8*1+3]*I.s3)+
            (Sinv[8*1+4]*I.s4)+
            (Sinv[8*1+5]*I.s5)+
            (Sinv[8*1+6]*I.s6)+
            (Sinv[8*1+7]*I.s7);
    
    n.z =   (Sinv[8*2+0]*I.s0)+
            (Sinv[8*2+1]*I.s1)+
            (Sinv[8*2+2]*I.s2)+
            (Sinv[8*2+3]*I.s3)+
            (Sinv[8*2+4]*I.s4)+
            (Sinv[8*2+5]*I.s5)+
            (Sinv[8*2+6]*I.s6)+
            (Sinv[8*2+7]*I.s7);
    return normalize(n);
}

__kernel void calcNormals(__read_only image2d_t img1, __read_only image2d_t img2, __read_only image2d_t img3, __read_only image2d_t img4, __read_only image2d_t img5, __read_only image2d_t img6, __read_only image2d_t img7, __read_only image2d_t img8, int width, int height, __global float *Sinv, __global float *P, __global float *Q, __global float *N) {
    
    /* get current i,j position in image */
    int i = get_global_id(0);
    int j = get_global_id(1);
    
    /* calculate surface normal */
    uchar8 I = getIntensityVector(i, j, img1, img2, img3, img4, img5, img6, img7, img8);
    float4 n = getNormalVector(Sinv, I);
    
    /* n = [-s1, -s2, 1]^T [Jaehne2005DBV] */
    P[(i*width*1)+(j*1)+(0)] = n.x/n.z;
    Q[(i*width*1)+(j*1)+(0)] = n.y/n.z;
    
    /* offset: (row * numCols * numChannels) + (col * numChannels) + (channel) */
    N[(i*width*3)+(j*3)+(0)] = n.x;
    N[(i*width*3)+(j*3)+(1)] = n.y;
    N[(i*width*3)+(j*3)+(2)] = n.z;
}

__kernel void integrate(__global float *P, __global float *Q, __global float *Z, int width, int height) {
    
    /* get current i,j position in image */
    int i  = get_global_id(0);
    int j  = get_global_id(1);
    
    if ( i != 0 || j != 0) {
        float u = sin((float)(i*2*M_PI_F/height));
        float v = sin((float)(j*2*M_PI_F/width));
        float uv = u*u + v*v;
        float d = uv;
        /* offset = (row * numCols * numChannels) + (col * numChannels) + channel */
        Z[(i*width*2)+(j*2)+(0)] = (u*P[(i*width*2)+(j*2)+(1)]  + v*Q[(i*width*2)+(j*2)+(1)]) / d;
        Z[(i*width*2)+(j*2)+(1)] = (-u*P[(i*width*2)+(j*2)+(0)] - v*Q[(i*width*2)+(j*2)+(0)]) / d;
    }
}