#include "OCR.h"
#include "ocr/OCRModel.h"
#include "ocr/OCRInternal.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
#ifndef OCR_THRESHOLD_BIAS
#define OCR_THRESHOLD_BIAS 7
#endif
#ifdef OCR_DEBUG
#include <stdio.h>
#endif

enum { MAX_PIXELS = 320*240, MAX_SIDE = 320, MAX_COMPONENTS = 256,
       MAX_STACK = 4096, MAX_LINES = 16, MAX_TEXT = 256, BOX_RADIUS = 12 };
typedef struct { uint16_t x0,y0,x1,y1,area; uint8_t punctuation, used, line; } Glyph;
typedef struct { uint16_t y0,y1; } Line;
static struct {
    uint8_t mask[MAX_PIXELS/8], visited[MAX_PIXELS/8];
    uint32_t stack[MAX_STACK];
    uint16_t column_sum[MAX_SIDE];
    Glyph glyph[MAX_COMPONENTS];
    Line line[MAX_LINES];
    uint8_t normalized[OCR_INPUT];
    float hidden[OCR_HIDDEN], scores[OCR_CLASSES];
    char text[MAX_TEXT];
} work;
static bool ready;
static OCRConfig config;
static OCRDiagnostics diagnostics;
static unsigned width, height, image_width, image_height;
static const CameraFrame *image;
static const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static bool bit_get(const uint8_t *bits, unsigned index) { return (bits[index>>3] >> (index&7)) & 1u; }
static void bit_set(uint8_t *bits, unsigned index) { bits[index>>3] |= 1u << (index&7); }
static void bit_clear(uint8_t *bits, unsigned index) { bits[index>>3] &= ~(1u << (index&7)); }
static unsigned minimum(unsigned a, unsigned b) { return a < b ? a : b; }
static unsigned maximum(unsigned a, unsigned b) { return a > b ? a : b; }

#ifdef OCR_DEBUG
static void debug_image(const char *name,const uint8_t *pixels,unsigned w,unsigned h) {
    FILE *f=fopen(name,"wb"); if (!f) return;
    fprintf(f,"P5\n%u %u\n255\n",w,h); fwrite(pixels,1,w*h,f); fclose(f);
}
#endif

static uint8_t luminance(unsigned x, unsigned y) {
    x += config.x; y += config.y;
    unsigned sx=x, sy=y;
    switch (config.rotation) {
        case 1: sx=y; sy=image->height-1-x; break;
        case 2: sx=image->width-1-x; sy=image->height-1-y; break;
        case 3: sx=image->width-1-y; sy=x; break;
        default: break;
    }
    unsigned offset = (sy*image->width+sx)*2u;
    unsigned pixel = (unsigned)image->data[offset]<<8 | image->data[offset+1];
    unsigned r=((pixel>>11)&31)*255/31, g=((pixel>>5)&63)*255/63, b=(pixel&31)*255/31;
    return (uint8_t)((77*r+150*g+29*b)>>8);
}

/* Sliding box sums avoid a full grayscale/integral-image allocation. */
static void binarize(void) {
    memset(work.mask,0,sizeof work.mask);
    memset(work.visited,0,sizeof work.visited);
    memset(work.column_sum,0,sizeof work.column_sum);
    for (unsigned y=0;y<minimum(height,BOX_RADIUS+1);++y) {
        for (unsigned x=0;x<width;++x) { work.column_sum[x]+=luminance(x,y); }
    }
    for (unsigned y=0;y<height;++y) {
        if (y) {
            if (y>BOX_RADIUS) for (unsigned x=0;x<width;++x) work.column_sum[x]-=luminance(x,y-BOX_RADIUS-1);
            if (y+BOX_RADIUS<height) for (unsigned x=0;x<width;++x) work.column_sum[x]+=luminance(x,y+BOX_RADIUS);
        }
        unsigned vertical = minimum(height,y+BOX_RADIUS+1)-(y>BOX_RADIUS?y-BOX_RADIUS:0);
        uint32_t sum=0;
        for (unsigned x=0;x<minimum(width,BOX_RADIUS+1);++x) sum+=work.column_sum[x];
        for (unsigned x=0;x<width;++x) {
            if (x) {
                if (x>BOX_RADIUS) sum-=work.column_sum[x-BOX_RADIUS-1];
                if (x+BOX_RADIUS<width) sum+=work.column_sum[x+BOX_RADIUS];
            }
            unsigned horizontal = minimum(width,x+BOX_RADIUS+1)-(x>BOX_RADIUS?x-BOX_RADIUS:0);
            unsigned mean=sum/(vertical*horizontal), value=luminance(x,y);
            bool ink=config.light_text ? value>mean+OCR_THRESHOLD_BIAS : value+OCR_THRESHOLD_BIAS<mean;
            if (ink) bit_set(work.mask,y*width+x);
        }
        // Notebook rules/underlines spanning >1/3 of the ROI are not characters.
        unsigned run=0;
        for (unsigned x=0;x<=width;++x) {
            if (x<width && bit_get(work.mask,y*width+x)) { ++run; }
            else {
                if (run>width/3 && run>20) {
                    for (unsigned k=x-run;k<x;++k) bit_clear(work.mask,y*width+k);
                }
                run=0;
            }
        }
    }
}

/* Ruled-paper lines can slope and contain gaps, so a horizontal run test
 * alone misses them. Vote over bounded slopes and remove long supported
 * baselines, retaining pixels at vertical character strokes. Reuse the
 * visited bitset as an immutable mask snapshot (no extra frame allocation). */
static void remove_rules(void) {
    unsigned best_score=0; int best_slope=0;
    static uint16_t histogram[MAX_SIDE+82];
    static uint16_t left[MAX_SIDE+82],right[MAX_SIDE+82];
    static uint32_t intensity[MAX_SIDE+82];
    for (int slope=-8;slope<=8;++slope) {
        memset(histogram,0,sizeof histogram);
        memset(left,0,sizeof left); memset(right,0,sizeof right);
        for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) {
            if (bit_get(work.mask,y*width+x)) {
                int intercept=(int)y-((int)x*slope)/64+41;
                if (intercept>=1 && intercept<MAX_SIDE+81 && x>5 && x+5<width) {
                    ++histogram[intercept];
                    if (x<width/8) ++left[intercept];
                    if (x>width*7/8) ++right[intercept];
                }
            }
        }
        unsigned score=0;
        for (unsigned y=1;y<MAX_SIDE+81;++y) {
            unsigned support=histogram[y-1]+histogram[y]+histogram[y+1];
            if (support>width/2 && (unsigned)(left[y-1]+left[y]+left[y+1])>width/40 &&
                (unsigned)(right[y-1]+right[y]+right[y+1])>width/40) score+=support;
        }
        if (score>best_score) { best_score=score; best_slope=slope; }
    }
    if (!best_score) return;
    memset(histogram,0,sizeof histogram);
    memset(left,0,sizeof left); memset(right,0,sizeof right);
    memset(intensity,0,sizeof intensity);
    memcpy(work.visited,work.mask,sizeof work.mask);
    for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) {
        if (bit_get(work.mask,y*width+x)) {
            int intercept=(int)y-((int)x*best_slope)/64+41;
            if (intercept>=1 && intercept<MAX_SIDE+81 && x>5 && x+5<width) {
                ++histogram[intercept]; intensity[intercept]+=luminance(x,y);
                if (x<width/8) ++left[intercept];
                if (x>width*7/8) ++right[intercept];
            }
        }
    }
    unsigned rule_groups=0,last_rule=0;
    for (unsigned y=1;y<MAX_SIDE+81;++y) {
        unsigned support=histogram[y-1]+histogram[y]+histogram[y+1];
        if (support>width/2 && (unsigned)(left[y-1]+left[y]+left[y+1])>width/40 &&
            (unsigned)(right[y-1]+right[y]+right[y+1])>width/40) {
            if (!rule_groups || y>last_rule+BOX_RADIUS) { ++rule_groups; last_rule=y; }
        }
    }
    // One text baseline (or two page edges) is not ruled paper.
    if (rule_groups<3) { memset(work.visited,0,sizeof work.visited); return; }
    for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) {
        int intercept=(int)y-((int)x*best_slope)/64+41;
        if (intercept<4 || intercept>=MAX_SIDE+78) continue;
        bool rule=false;
        for (int d=-2;d<=2;++d) {
            int p=intercept+d;
            if ((unsigned)(histogram[p-1]+histogram[p]+histogram[p+1])>width/2 &&
                (unsigned)(left[p-1]+left[p]+left[p+1])>width/40 &&
                (unsigned)(right[p-1]+right[p]+right[p+1])>width/40) rule=true;
        }
        if (!rule) continue;
        uint32_t sum=0,count=0;
        for (int d=-3;d<=3;++d) { sum+=intensity[intercept+d]; count+=histogram[intercept+d]; }
        unsigned reference=count?sum/count:0,value=luminance(x,y);
        // Printed rules are lighter than the ink in these camera captures.
        // Keep dark ink crossing a rule instead of amputating L/E/O strokes.
        bool strong=config.light_text?value>reference+12:value+12<reference;
        if (!strong) bit_clear(work.mask,y*width+x);
    }
    // Close one-pixel threshold gaps without expanding separate glyphs.
    memcpy(work.visited,work.mask,sizeof work.mask);
    for (unsigned y=1;y+1<height;++y) for (unsigned x=1;x+1<width;++x) {
        unsigned p=y*width+x;
        if ((bit_get(work.visited,p-1) && bit_get(work.visited,p+1)) ||
            (bit_get(work.visited,p-width) && bit_get(work.visited,p+width))) bit_set(work.mask,p);
    }
    memset(work.visited,0,sizeof work.visited);
}

static bool add_neighbor(unsigned index, unsigned *top) {
    if (!bit_get(work.mask,index) || bit_get(work.visited,index)) return true;
    if (*top>=MAX_STACK) return false;
    bit_set(work.visited,index); work.stack[(*top)++]=index;
    return true;
}

static bool components(void) {
    for (unsigned index=0;index<width*height;++index) {
        if (!bit_get(work.mask,index) || bit_get(work.visited,index)) continue;
        unsigned top=0,area=0,x0=width,y0=height,x1=0,y1=0;
        if (!add_neighbor(index,&top)) return false;
        while (top) {
            unsigned p=work.stack[--top],x=p%width,y=p/width;
            ++area; x0=minimum(x0,x); x1=maximum(x1,x); y0=minimum(y0,y); y1=maximum(y1,y);
            for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx) {
                int nx=(int)x+dx, ny=(int)y+dy;
                if (nx>=0 && ny>=0 && nx<(int)width && ny<(int)height &&
                    !add_neighbor((unsigned)ny*width+(unsigned)nx,&top)) return false;
            }
        }
        unsigned w=x1-x0+1,h=y1-y0+1;
        if (area<2 || h>height*3/4 || w>width*2/3 || w>h*4 || area>65535 ||
            x0==0 || y0==0 || x1+1==width || y1+1==height) continue;
        if (diagnostics.components>=MAX_COMPONENTS) return false;
        work.glyph[diagnostics.components++] = (Glyph){x0,y0,x1,y1,(uint16_t)area,0,0,0};
    }
    return true;
}

static void split_joined(void) {
    unsigned original=diagnostics.components;
    for (unsigned i=0;i<original;++i) {
        Glyph remaining=work.glyph[i];
        unsigned h=remaining.y1-remaining.y0+1,w=remaining.x1-remaining.x0+1;
        if (h<8 || w*10<h*14) continue;
#ifdef OCR_DEBUG
        fprintf(stderr,"SPLIT root=%u x=%u y=%u w=%u h=%u\n",i,remaining.x0,remaining.y0,w,h);
#endif
        // Scale width to the current component's height. Nearby letters can
        // be smaller than the digits, so copying their raw widths over-splits.
        unsigned typical=maximum(4,h*3/5);
        unsigned parts=maximum(2,(w+typical/2)/typical);
#ifdef OCR_DEBUG
        fprintf(stderr,"SPLIT typical=%u parts=%u\n",typical,parts);
#endif
        if (parts>8) { work.glyph[i].punctuation=2; continue; }
        bool changed=false;
        for (unsigned part_index=1;part_index<parts && diagnostics.components<MAX_COMPONENTS;++part_index) {
            unsigned best=0,score=height+1;
            unsigned target=work.glyph[i].x0+w*part_index/parts,radius=maximum(2,h/10);
            unsigned begin=maximum(remaining.x0+3,target>radius?target-radius:0);
            unsigned end=minimum(remaining.x1-3,target+radius);
            for (unsigned x=begin;x<=end;++x) {
                unsigned ink=0;
                for (unsigned y=remaining.y0;y<=remaining.y1;++y) ink+=bit_get(work.mask,y*width+x);
                if (ink<score) { score=ink; best=x; }
            }
            if (!best || score>maximum(2,h*2/3)) break;
            Glyph part=remaining; part.x1=(uint16_t)(best-1);
            unsigned area=0;
            for (unsigned y=part.y0;y<=part.y1;++y) for (unsigned x=part.x0;x<=part.x1;++x)
                area+=bit_get(work.mask,y*width+x);
            part.area=(uint16_t)area;
            work.glyph[diagnostics.components++]=part;
            remaining.x0=(uint16_t)(best+1); changed=true;
        }
        if (changed) work.glyph[i]=remaining;
        if ((unsigned)(remaining.x1-remaining.x0+1)*10>h*14) {
            work.glyph[i].punctuation=2;
        }
    }
}

static void normalize(const Glyph *g) {
    memset(work.normalized,0,sizeof work.normalized);
    Glyph tight=*g; unsigned left=g->x1,top=g->y1,right=g->x0,bottom=g->y0; bool any=false;
    for (unsigned y=g->y0;y<=g->y1;++y) for (unsigned x=g->x0;x<=g->x1;++x) {
        if (bit_get(work.mask,y*width+x)) {
            left=minimum(left,x); right=maximum(right,x); top=minimum(top,y); bottom=maximum(bottom,y); any=true;
        }
    }
    if (!any) return;
    tight.x0=(uint16_t)left; tight.x1=(uint16_t)right; tight.y0=(uint16_t)top; tight.y1=(uint16_t)bottom; g=&tight;
    unsigned w=g->x1-g->x0+1,h=g->y1-g->y0+1;
    unsigned nw=w>=h?OCR_EXTENT:maximum(1,w*OCR_EXTENT/h), nh=h>=w?OCR_EXTENT:maximum(1,h*OCR_EXTENT/w);
    unsigned ox=(OCR_SIDE-nw)/2,oy=(OCR_SIDE-nh)/2;
    for (unsigned y=0;y<nh;++y) for (unsigned x=0;x<nw;++x) {
        unsigned x0=x*w/nw, x1=minimum(w,((x+1)*w+nw-1)/nw);
        unsigned y0=y*h/nh, y1=minimum(h,((y+1)*h+nh-1)/nh), sum=0;
        for (unsigned yy=y0;yy<y1;++yy) for (unsigned xx=x0;xx<x1;++xx) {
            sum+=bit_get(work.mask,(g->y0+yy)*width+g->x0+xx);
        }
        work.normalized[(oy+y)*OCR_SIDE+ox+x]=(uint8_t)(sum*255/((x1-x0)*(y1-y0)));
    }
}

static char classify(const uint8_t pixels[OCR_INPUT], uint8_t *confidence,unsigned mode) {
    if (!pixels || !confidence) return '?';
    memcpy(work.hidden,ocr_b1,sizeof work.hidden);
    for (unsigned i=0;i<OCR_INPUT;++i) if (pixels[i]) {
        float input=(float)pixels[i]/255.0f;
        for (unsigned j=0;j<OCR_HIDDEN;++j) work.hidden[j]+=input*ocr_w1[i*OCR_HIDDEN+j];
    }
    memcpy(work.scores,ocr_b2,sizeof work.scores);
    for (unsigned j=0;j<OCR_HIDDEN;++j) if (work.hidden[j]>0) {
        for (unsigned k=0;k<OCR_CLASSES;++k) work.scores[k]+=work.hidden[j]*ocr_w2[j*OCR_CLASSES+k];
    }
    unsigned start=mode==1?10:0,stop=mode==2?10:OCR_CLASSES;
    unsigned best=start,second=start+1;
    for (unsigned k=start+1;k<stop;++k) if (work.scores[k]>work.scores[best]) best=k;
    second=best==start?start+1:start;
    for (unsigned k=start;k<stop;++k) if (k!=best && work.scores[k]>work.scores[second]) second=k;
    float sum=0;
    for (unsigned k=start;k<stop;++k) sum+=expf(work.scores[k]-work.scores[best]);
    float probability=1.0f/sum;
    float margin=probability*(1.0f-expf(work.scores[second]-work.scores[best]));
    *confidence=(uint8_t)(probability*100.0f);
#ifdef OCR_DEBUG
    fprintf(stderr,"top=%c second=%c p=%u margin=%u ",alphabet[best],alphabet[second],*confidence,(unsigned)(margin*100));
#endif
    return probability>=.80f && margin>=.35f ? alphabet[best] : '?';
}

char ocr_classify_glyph(const uint8_t pixels[OCR_INPUT],uint8_t *confidence) {
    return classify(pixels,confidence,0);
}

static bool arrange(void) {
    unsigned heights[MAX_COMPONENTS],count=0;
    for (unsigned i=0;i<diagnostics.components;++i) {
        unsigned h=work.glyph[i].y1-work.glyph[i].y0+1;
        if (h>=6) heights[count++]=h;
    }
    if (!count) return false;
    for (unsigned i=1;i<count;++i) {
        unsigned n=heights[i],j=i;
        while (j && heights[j-1]>n) { heights[j]=heights[j-1]; --j; }
        heights[j]=n;
    }
    unsigned typical=heights[(count-1)*3/4];
    // Two aligned small components may form a colon; never synthesize one
    // from a missing punctuation mark or assume a fixed message grammar.
    for (unsigned i=0;i<diagnostics.components;++i) {
        Glyph *a=&work.glyph[i];
        if (a->used || (unsigned)(a->y1-a->y0+1)>typical/3 || (unsigned)(a->x1-a->x0+1)>typical/3) continue;
        for (unsigned j=i+1;j<diagnostics.components;++j) {
            Glyph *b=&work.glyph[j];
            int dx=(int)(a->x0+a->x1)-(int)(b->x0+b->x1);
            unsigned gap=b->y0>a->y1?b->y0-a->y1:0;
            if (!b->used && (unsigned)(b->y1-b->y0+1)<=typical/3 && (unsigned)(b->x1-b->x0+1)<=typical/3 &&
                dx>=-4 && dx<=4 && gap>=2 && gap<typical) {
                a->x0=minimum(a->x0,b->x0); a->x1=maximum(a->x1,b->x1);
                a->y1=b->y1; a->punctuation=1; b->used=1; break;
            }
        }
    }
    for (unsigned i=0;i<diagnostics.components;++i) {
        Glyph *g=&work.glyph[i];
        unsigned h=g->y1-g->y0+1;
        if (g->used || (!g->punctuation && (h<6 || h<typical/2 || g->area<5))) { g->used=1; continue; }
        unsigned line=0;
        for (;line<diagnostics.lines;++line) {
            Line *l=&work.line[line];
            unsigned overlap=minimum(g->y1,l->y1);
            unsigned start=maximum(g->y0,l->y0);
            if (overlap>=start && overlap-start+1>=minimum(h,l->y1-l->y0+1)/3) break;
        }
        if (line==diagnostics.lines) {
            if (line>=MAX_LINES) return false;
            work.line[line]=(Line){g->y0,g->y1}; ++diagnostics.lines;
        } else {
            work.line[line].y0=minimum(work.line[line].y0,g->y0);
            work.line[line].y1=maximum(work.line[line].y1,g->y1);
        }
        g->line=(uint8_t)line; ++diagnostics.glyphs;
    }
    for (unsigned l=0;l<diagnostics.lines;++l) {
        bool real=false;
        for (unsigned i=0;i<diagnostics.components;++i)
            if (!work.glyph[i].used && work.glyph[i].line==l && !work.glyph[i].punctuation) real=true;
        if (!real) for (unsigned i=0;i<diagnostics.components;++i) {
            if (!work.glyph[i].used && work.glyph[i].line==l) { work.glyph[i].used=1; --diagnostics.glyphs; }
        }
    }
    if (config.single_line && diagnostics.lines>1) {
        unsigned best=0; int best_score=-100000;
        for (unsigned l=0;l<diagnostics.lines;++l) {
            int score=0;
            for (unsigned i=0;i<diagnostics.components;++i) {
                Glyph *g=&work.glyph[i];
                if (!g->used && g->line==l && !g->punctuation) {
                    uint8_t confidence; normalize(g); classify(work.normalized,&confidence,0);
                    score+=confidence>=80?(int)confidence-50:(int)confidence-80;
                }
            }
            if (score>best_score) { best=l; best_score=score; }
        }
        for (unsigned i=0;i<diagnostics.components;++i) {
            if (!work.glyph[i].used && work.glyph[i].line!=best) { work.glyph[i].used=1; --diagnostics.glyphs; }
        }
    }
    return diagnostics.glyphs!=0;
}

AppStatus ocr_init(void) {
    ready=true; memset(&config,0,sizeof config); memset(&diagnostics,0,sizeof diagnostics);
    return APP_OK;
}
AppStatus ocr_configure(const OCRConfig *settings) {
    if (!settings || settings->rotation>3 || (!settings->width != !settings->height) ||
        (!settings->width && (settings->x || settings->y))) return APP_ERROR;
    if (!ready) return APP_NOT_READY;
    config=*settings; return APP_OK;
}
AppStatus ocr_get_diagnostics(OCRDiagnostics *result) {
    if (!result) return APP_ERROR;
    *result=diagnostics; return ready?APP_OK:APP_NOT_READY;
}

AppStatus ocr_read_frame(const CameraFrame *frame,char *buffer,uint16_t capacity) {
    if (!buffer || !capacity) return APP_ERROR;
    buffer[0]='\0'; memset(&diagnostics,0,sizeof diagnostics);
    if (!ready) return APP_NOT_READY;
    if (!frame || !frame->data || !frame->width || !frame->height ||
        frame->width>MAX_SIDE || frame->height>MAX_SIDE ||
        (uint32_t)frame->width*frame->height>MAX_PIXELS ||
        frame->length!=(uint32_t)frame->width*frame->height*2) return APP_ERROR;
    image=frame;
    image_width=(config.rotation&1)?frame->height:frame->width;
    image_height=(config.rotation&1)?frame->width:frame->height;
    width=config.width?config.width:image_width;
    height=config.height?config.height:image_height;
    if ((unsigned)config.x+width>image_width || (unsigned)config.y+height>image_height || width<3 || height<3)
        return APP_ERROR;
    binarize();
#ifndef OCR_SKIP_RULES
    if (!config.single_line) remove_rules();
#endif
#ifdef OCR_DEBUG
    { FILE *f=fopen("build/ocr-mask.pgm","wb");
      if (f) { fprintf(f,"P5\n%u %u\n255\n",width,height);
        for (unsigned i=0;i<width*height;++i) fputc(bit_get(work.mask,i)?0:255,f);
        fclose(f); } }
#endif
    if (!components()) return APP_NOT_READY;
    split_joined();
    if (!arrange()) return APP_NOT_READY;
    diagnostics.minimum_confidence=100;
    unsigned length=0;
    // Lines were found top-to-bottom by the component scan. Within each,
    // choose the leftmost unused glyph; component count is strictly bounded.
    for (unsigned l=0;l<diagnostics.lines;++l) {
        bool first=true; unsigned last_right=0;
        // Infer the ordinary inter-character gap on this line instead of
        // treating narrow handwritten letters as separate words.
        unsigned gaps[MAX_COMPONENTS], gap_count=0, previous_right=0;
        bool have_previous=false;
        for (unsigned scan=0;scan<diagnostics.glyphs;++scan) {
            unsigned next=MAX_COMPONENTS;
            for (unsigned i=0;i<diagnostics.components;++i) {
                Glyph *g=&work.glyph[i];
                if (!g->used && g->line==l && (!have_previous || g->x0>previous_right) &&
                    (next==MAX_COMPONENTS || g->x0<work.glyph[next].x0)) next=i;
            }
            if (next==MAX_COMPONENTS) break;
            if (have_previous) gaps[gap_count++]=work.glyph[next].x0-previous_right-1;
            previous_right=work.glyph[next].x1; have_previous=true;
        }
        for (unsigned i=1;i<gap_count;++i) {
            unsigned n=gaps[i],j=i;
            while (j && gaps[j-1]>n) { gaps[j]=gaps[j-1]; --j; }
            gaps[j]=n;
        }
        unsigned word_gap=maximum(3,(unsigned)(work.line[l].y1-work.line[l].y0+1)/4);
        if (gap_count>=2) word_gap=maximum(word_gap,gaps[(gap_count-1)/2]*2);
        unsigned mode=0,word_end=0;
        for (;;) {
            unsigned best=MAX_COMPONENTS;
            for (unsigned i=0;i<diagnostics.components;++i) {
                Glyph *g=&work.glyph[i];
                if (!g->used && g->line==l && (best==MAX_COMPONENTS || g->x0<work.glyph[best].x0)) best=i;
            }
            if (best==MAX_COMPONENTS) break;
            Glyph *g=&work.glyph[best]; g->used=1;
            if (g->punctuation==2) { ++diagnostics.rejected; continue; }
            if (first || g->x0>word_end) {
                unsigned letters=0,digits=0,anchors=0;
                word_end=g->x1;
                bool extended=true;
                while (extended) {
                    extended=false;
                    for (unsigned j=0;j<diagnostics.components;++j) {
                        Glyph *candidate=&work.glyph[j];
                        if ((!candidate->used || j==best) && candidate->line==l &&
                            candidate->x0>=g->x0 && candidate->x0<=word_end+1+word_gap && candidate->x1>word_end) {
                            word_end=candidate->x1; extended=true;
                        }
                    }
                }
                for (unsigned j=0;j<diagnostics.components;++j) {
                    Glyph *candidate=&work.glyph[j];
                    if ((!candidate->used || j==best) && candidate->line==l && !candidate->punctuation &&
                        candidate->x0>=g->x0 && candidate->x1<=word_end) {
                        uint8_t c; normalize(candidate); classify(work.normalized,&c,0);
                        unsigned top=0,second=1;
                        for (unsigned k=1;k<OCR_CLASSES;++k) if (work.scores[k]>work.scores[top]) top=k;
                        second=top?0:1;
                        for (unsigned k=0;k<OCR_CLASSES;++k) if (k!=top && work.scores[k]>work.scores[second]) second=k;
                        // O/0 and I/1 cannot vote for their own interpretation.
                        if (c>=60 && work.scores[top]-work.scores[second]>.8f &&
                            top!=0 && top!=1 && top!=18 && top!=24) {
                            if (top<10) ++digits; else ++letters;
                            ++anchors;
                        }
                    }
                }
                // Require a clear majority, so mixed tokens such as AB12
                // remain unrestricted. This can resolve a curled handwritten
                // 1 misread as L when two other digits agree on the token.
                mode=letters>=2 && letters>=2*digits?1:digits>=2 && digits>=2*letters?2:0;
            }
            char character=':'; uint8_t confidence=100;
            if (!g->punctuation) {
                normalize(g); character=classify(work.normalized,&confidence,0);
                // Context may resolve uncertain glyphs and visual O/0/I/1
                // ambiguity; never turn a confident B in B12 into an 8.
                if (mode && (character=='?' || character=='O' || character=='0' || character=='I' || character=='1'))
                    character=classify(work.normalized,&confidence,mode);
            }
#ifdef OCR_DEBUG
            if (!g->punctuation) {
                char path[128]; snprintf(path,sizeof path,"build/ocr-glyph-%u.pgm",best);
                debug_image(path,work.normalized,OCR_SIDE,OCR_SIDE);
                fprintf(stderr,"glyph %u box=%u,%u,%u,%u char=%c confidence=%u\n",best,
                        g->x0,g->y0,g->x1,g->y1,character,confidence);
            }
#endif
            diagnostics.minimum_confidence=(uint8_t)minimum(diagnostics.minimum_confidence,confidence);
            if (character=='?') { ++diagnostics.rejected; continue; }
            if (length+3>=MAX_TEXT) return APP_NOT_READY;
            if (first && l && length) work.text[length++]='\n';
            if (!first && g->x0>last_right+1+word_gap)
                work.text[length++]=' ';
            work.text[length++]=character;
            first=false; last_right=g->x1;
        }
    }
    if (!length || diagnostics.rejected) return APP_NOT_READY;
    if (length+1>capacity) return APP_ERROR;
    work.text[length]='\0'; memcpy(buffer,work.text,length+1);
    return APP_OK;
}

AppStatus ocr_read_text(char *buffer,uint16_t capacity) {
    if (!buffer || !capacity) return APP_ERROR;
    buffer[0]='\0';
    if (!ready) return APP_NOT_READY;
    CameraFrame frame;
    AppStatus status=camera_get_frame(&frame);
    return status==APP_OK?ocr_read_frame(&frame,buffer,capacity):status;
}
