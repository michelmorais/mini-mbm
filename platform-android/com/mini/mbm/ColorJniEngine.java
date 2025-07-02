package com.mini.mbm;

public class ColorJniEngine 
{
	public 	float 	r, g, b, a;
	private static byte[] rgba = new byte[4];
	private static final float cbyte = 1.0f/255.0f;
	
	@SuppressWarnings("unused")
    public ColorJniEngine() 
	{
		r = g = b = a = 0.0f;
	}
    
	@SuppressWarnings("unused")
	public ColorJniEngine( final float pf[] )
	{
		r = pf[0];
		g = pf[1];
		b = pf[2];
		a = pf[3];
	}
	
	public ColorJniEngine( float fr, float fg, float fb, float fa )
	{
		r = fr;
		g = fg;
		b = fb;
		a = fa;
	}
	
	@SuppressWarnings("unused")
	public ColorJniEngine( int ir, int ig, int ib, int ia )
	{
		if(ir == 1)
			r = 1;
		else
			r = cbyte * (float) (ir);
		if(ig ==1)
			g = 1;
		else
			g = cbyte * (float) (ig);
		if(ib ==1)
			b = 1;
		else
			b = cbyte * (float) (ib);
		if(ia==1)
			a = 1;
		else
			a = cbyte * (float) (ia);
	}
	
	@SuppressWarnings("unused")
	public void getRGBA(byte[] RGBA)//RGBA  byte (0 - 255)
	{
		RGBA[0] = ( byte)(r * 255.0f + 0.5f);
		RGBA[1]	= ( byte)(g * 255.0f + 0.5f);
		RGBA[2]	= ( byte)(b * 255.0f + 0.5f);
		RGBA[4]	= ( byte)(a * 255.0f + 0.5f);
	}
	
	public void getRGB(byte[] RGB)//RGB  byte (0 - 255)
	{
		RGB[0] 	= ( byte)(r * 255.0f + 0.5f);
		RGB[1]	= ( byte)(g * 255.0f + 0.5f);
		RGB[2]	= ( byte)(b * 255.0f + 0.5f);
	}
	
	@SuppressWarnings("unused")
	public byte[] getRGBA()//RGBA  byte (0 - 255) 0=r 1=g 2=b 3=a
	{
		getRGB(rgba);
		return rgba;
	}
	
	@SuppressWarnings("unused")
	public ColorJniEngine add ( final ColorJniEngine c )
	{
		r += c.r;
		g += c.g;
		b += c.b;
		a += c.a;
		return this;
	}
	
	@SuppressWarnings("unused")
	public ColorJniEngine sub (final ColorJniEngine c )
	{
		r -= c.r;
		g -= c.g;
		b -= c.b;
		a -= c.a;
		return this;
	}
	
	@SuppressWarnings("unused")
	public ColorJniEngine mult (final float f )
	{
		r *= f;
		g *= f;
		b *= f;
		a *= f;
		return this;
	}
	
	@SuppressWarnings("unused")
	public ColorJniEngine div (final float f )
	{
		float fInv = 1.0f / f;
		r *= fInv;
		g *= fInv;
		b *= fInv;
		a *= fInv;
		return this;
	}
	
	@SuppressWarnings("unused")
	public void set(float r, float g, float b, float a) 
	{
		this.r = r;
		this.g = g;
		this.b = b;
		this.a = a;
	}
	
	@SuppressWarnings("unused")
	public void set(int r, int g, int  b) 
	{
		this.r = r * cbyte;
		this.g = g * cbyte;
		this.b = b * cbyte;
	}
	
	@SuppressWarnings("unused")
	public void set(int r, int g, int  b, int a) 
	{
		this.r = r * cbyte;
		this.g = g * cbyte;
		this.b = b * cbyte;
		this.a = a * cbyte;
	}
	
	@SuppressWarnings("unused")
	public void set(ColorJniEngine newColor) 
	{
		this.r = newColor.r;
		this.g = newColor.g;
		this.b = newColor.b;
		this.a = newColor.a;
	}
	
	@SuppressWarnings("unused")
    public float dot(final float x,final float y,final float z)
    {
    	return this.r * x + this.g * y + this.b * z;
    }
  	
  	@SuppressWarnings("unused")
    public float dot(final float x,final float y,final float z,final float w)
    {
    	return this.r * x + this.g * y + this.b * z + this.a * w;
    }
}
