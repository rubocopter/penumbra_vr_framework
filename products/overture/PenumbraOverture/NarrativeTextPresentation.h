#pragma once

// Source-owned HPL font boundary. Include after the game/engine declarations.
// Same centered 2x narrative layout as BP/RQ; native HPL wrapping, text,
// duration, color/fades and panel/world transforms retain their owners.
inline float VrNarrativeBlockHeight(hpl::iFontData* font,const hpl::tWString& text,
    float width,float line,hpl::cVector2f size)
{
    hpl::tWStringVec rows;
    font->GetWordWrapRows(width,line,size,text,&rows);
    return size.y+(rows.empty()?0.0f:static_cast<float>(rows.size()-1)*line);
}

inline void DrawVrNarrativeWordWrap(hpl::iFontData* font,hpl::cVector3f position,
    float width,float line,hpl::cVector2f size,const hpl::cColor& color,
    const hpl::tWString& text,bool vr,hpl::cVector2f nativeOrigin)
{
    if(vr)
    {
        size=size*2.0f;
        line*=2.0f;
        position.x+=400.0f-nativeOrigin.x;
        position.y+=300.0f-nativeOrigin.y-
            VrNarrativeBlockHeight(font,text,width,line,size)*0.5f;
    }
    font->DrawWordWrap(position,width,line,size,color,
        vr?hpl::eFontAlign_Center:hpl::eFontAlign_Left,text);
}

inline void DrawVrNarrativeWorldText(hpl::iFontData* font,hpl::cVector3f position,
    float width,float line,hpl::cVector2f size,const hpl::cColor& color,
    const hpl::tWString& text,const hpl::cMatrixf& transform,float subtitleScale,bool vr)
{
    if(vr)
    {
        size=size*(2.0f*subtitleScale);
        line*=2.0f*subtitleScale;
        position.y-=VrNarrativeBlockHeight(font,text,width,line,size)*0.5f;
    }
    font->DrawWordWrapWorld(position,width,line,size,color,
        hpl::eFontAlign_Center,text,transform);
}
