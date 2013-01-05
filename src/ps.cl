
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

__kernel void calcNormals(__read_only image2d_t img1, __read_only image2d_t img2, __read_only image2d_t img3, __read_only image2d_t img4, __read_only image2d_t img5, __read_only image2d_t img6, __read_only image2d_t img7, __read_only image2d_t img8, int width, int height, __global float *Sinv, __global float *P, __global float *Q, __global float *N, float maxpq, float slope) {
    
    /* get current i,j position in image */
    int i = get_global_id(0);
    int j = get_global_id(1);
    
    /* calculate surface normal */
    uchar8 I = getIntensityVector(i, j, img1, img2, img3, img4, img5, img6, img7, img8);
    float4 n = getNormalVector(Sinv, I);
    
    /* exaggerate slope as in [Malzbender2006] */
    n.x *= slope;
    n.y *= slope;
    n.z = sqrt(1.0f - pow(n.x, 2) - pow(n.y, 2));
    n = normalize(n);
    
    /* updated depth gradients as in [Wei2001] */
    float p = n.x/n.z;
    float q = n.y/n.z;
    if (fabs(p) < maxpq && fabs(q) < maxpq) {
        P[(i*width*1)+(j*1)+(0)] = p;
        Q[(i*width*1)+(j*1)+(0)] = q;
    }
    
    /* offset: (row * numCols * numChannels) + (col * numChannels) + (channel) */
    N[(i*width*3)+(j*3)+(0)] = n.x;
    N[(i*width*3)+(j*3)+(1)] = n.y;
    N[(i*width*3)+(j*3)+(2)] = n.z;
}

__kernel void updateNormals(__global float *N, int width, int height, __global float *P, __global float *Q, float scale) {

    /* get current i,j position in image */
    int i  = get_global_id(0);
    int j  = get_global_id(1);
    
    /* avoid edges */
    if (i < 1 || j < 1) { return; }
    if (i == height-1 || j == width-1 ) { return; }
    
    /* get average of normal vectors over a 9x9 local patch */
    float4 nsum = (float4)(0.0f, 0.0f, 0.0f, 0.0f);
    for(int y = i-1; y <= i+1; y++) {
        for(int x = j-1; x <= j+1; x++) {
            nsum.x += N[(y*width*3)+(x*3)+(0)];
            nsum.y += N[(y*width*3)+(x*3)+(1)];
            nsum.z += N[(y*width*3)+(x*3)+(2)];
        }
    }
    
    /* unsharp masking normals [Malzbender2006] */
    float4 n;
    n.x = N[(i*width*3)+(j*3)+(0)];
    n.y = N[(i*width*3)+(j*3)+(1)];
    n.z = N[(i*width*3)+(j*3)+(2)];
    n = n + scale * (n - normalize(nsum));
    if (n.z < 0.0f) {n.z = 0.0f; }
    
    /* offset: (row * numCols * numChannels) + (col * numChannels) + (channel) */
    N[(i*width*3)+(j*3)+(0)] = n.x;
    N[(i*width*3)+(j*3)+(1)] = n.y;
    N[(i*width*3)+(j*3)+(2)] = n.z;
}

__kernel void integrate(__global float *P, __global float *Q, __global float *Z, int width, int height, float lambda, float mu) {
    
    /* get current i,j position in image */
    int i  = get_global_id(0);
    int j  = get_global_id(1);
    float u = sin((float)(i*2*M_PI_F/height));
    float v = sin((float)(j*2*M_PI_F/width));
    
    if ( u != 0 && v != 0) {
        float l = (1.0f + lambda)*(pow(u,2) + pow(v,2)) + mu*pow((pow(u,2)+pow(v,2)),2);
        /* offset = (row * numCols * numChannels) + (col * numChannels) + channel */
        float d1 =  u*P[(i*width*2)+(j*2)+(1)] + v*Q[(i*width*2)+(j*2)+(1)];
        float d2 = -u*P[(i*width*2)+(j*2)+(0)] - v*Q[(i*width*2)+(j*2)+(0)];
        Z[(i*width*2)+(j*2)+(0)] = d1 / l;
        Z[(i*width*2)+(j*2)+(1)] = d2 / l;
    }
}