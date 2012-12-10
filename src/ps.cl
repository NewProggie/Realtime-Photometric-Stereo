#pragma OPENCL EXTENSION cl_khr_fp64 : enable

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST; 

__kernel void calcNormals(__read_only image2d_t img1, __read_only image2d_t img2, __read_only image2d_t img3, __read_only image2d_t img4, __read_only image2d_t img5, __read_only image2d_t img6, __read_only image2d_t img7, __read_only image2d_t img8, int width, int height, __global float *Sinv, __global float *P, __global float *Q, __global float *N) {
    
    int x = get_global_id(0) % width;
    int y = (get_global_id(0) / width) % width;

    uchar8 I;
    I.s0 = read_imageui(img1, sampler, (int2)(x,y)).x;
    I.s1 = read_imageui(img2, sampler, (int2)(x,y)).x;
    I.s2 = read_imageui(img3, sampler, (int2)(x,y)).x;
    I.s3 = read_imageui(img4, sampler, (int2)(x,y)).x;
    I.s4 = read_imageui(img5, sampler, (int2)(x,y)).x;
    I.s5 = read_imageui(img6, sampler, (int2)(x,y)).x;
    I.s6 = read_imageui(img7, sampler, (int2)(x,y)).x;
    I.s7 = read_imageui(img8, sampler, (int2)(x,y)).x;
    
    float4 n;
    n.x = (Sinv[8*0+0]*I.s0) + (Sinv[8*0+1]*I.s1) + (Sinv[8*0+2]*I.s2) + (Sinv[8*0+3]*I.s3) + (Sinv[8*0+4]*I.s4) + (Sinv[8*0+5]*I.s5) + (Sinv[8*0+6]*I.s6) + (Sinv[8*0+7]*I.s7);
    n.y = (Sinv[8*1+0]*I.s0) + (Sinv[8*1+1]*I.s1) + (Sinv[8*1+2]*I.s2) + (Sinv[8*1+3]*I.s3) + (Sinv[8*1+4]*I.s4) + (Sinv[8*1+5]*I.s5) + (Sinv[8*1+6]*I.s6) + (Sinv[8*1+7]*I.s7);
    n.z = (Sinv[8*2+0]*I.s0) + (Sinv[8*2+1]*I.s1) + (Sinv[8*2+2]*I.s2) + (Sinv[8*2+3]*I.s3) + (Sinv[8*2+4]*I.s4) + (Sinv[8*2+5]*I.s5) + (Sinv[8*2+6]*I.s6) + (Sinv[8*2+7]*I.s7);
    float p = sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (p > 0) {
        n /= p;
    }
    
    if (n.z == 0) {
        n.z = 1.0f;
    }
    
    n = normalize(n);
    
    P[width*y+x] = -n.x;
    Q[width*y+x] = n.y;
    N[get_global_id(0)*3+0] = n.x;
    N[get_global_id(0)*3+1] = n.y;
    N[get_global_id(0)*3+2] = n.z;
}

__kernel void integrate(__global float *P, __global float *Q, __global float *Z, int width, int height) {
    
    int i = get_global_id(0) % width;
    int j = (get_global_id(0) / width) % width;
    float v = sin((float)(i*2*M_PI_F/height));
    float u = sin((float)(j*2*M_PI_F/width));
    float uv = u*u + v*v;
    float d = uv;
    /* offset = (row * numCols * numChannels) + (col * numChannels) + channel */
    Z[(i*width*2)+(j*2)+0] = (u*P[(i*width*2)+(j*2)+1] + v*Q[(i*width*2)+(j*2)+1]) / d;
    Z[(i*width*2)+(j*2)+1] = (-u*P[(i*width*2)+(j*2)+0] - v*Q[(i*width*2)+(j*2)+0]) / d;
}