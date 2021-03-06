
#include "stdafx.h"
#include "viewimage.h"
#include "threadrunner.h"
#include "viewergamma.h"

void ImageViewer::RunThread(int index)
{
	if (!m_img.rgb)
		return;

	GammaLutCalculator rg(m_gamma, m_exposure, m_defog * m_fogR, m_kneeLow, m_kneeHigh, m_GammaLut[index][0]);
	GammaLutCalculator gg(m_gamma, m_exposure, m_defog * m_fogG, m_kneeLow, m_kneeHigh, m_GammaLut[index][1]);
	GammaLutCalculator bg(m_gamma, m_exposure, m_defog * m_fogB, m_kneeLow, m_kneeHigh, m_GammaLut[index][2]);

	int ww = m_img.width;
	int hh = m_img.height / THREAD_NUM;
	int x_start = 0;
	int y_start = index * hh;
	int x_end = x_start + ww;
	int y_end = y_start + hh;
	Imf::Rgba *pixel0 = &(m_img.pixels[0]);
	Imf::Rgba *pixel;
	BYTE *color0 = (BYTE*)m_img.rgb;
	BYTE *color;

	pixel0 += m_img.width * y_start;
	color0 += m_img.width * y_start * 3;
	for (int y = y_start; y < y_end && y < m_img.height; y++)
	{
		pixel = pixel0 + x_start;
		color = color0 + x_start * 3;
		for (int x = x_start; x < x_end; x++)
		{
			*color++ = (unsigned char)(dither(bg.calc(pixel->b), x, y) /** 255.f*/);
			*color++ = (unsigned char)(dither(gg.calc(pixel->g), x, y) /** 255.f*/);
			*color++ = (unsigned char)(dither(rg.calc(pixel->r), x, y) /** 255.f*/);
			pixel++;
		}
		pixel0 += m_img.width;
		color0 += m_img.width * 3;
	}
}

void ImageViewer::CreateThreads()
{
	for (int i = 0; i < THREAD_NUM; i++)
	{
		m_Threads[i] = new ThreadRunner(this, i);
	}
}

void ImageViewer::DestroyThreads()
{
	for (int i = 0; i < THREAD_NUM; i++)
	{
		SAFEDELETE(m_Threads[i]);
	}
}

void ImageViewer::ResumeAllThreads()
{
	for (int i = 0; i < THREAD_NUM; i++)
	{
		m_Threads[i]->Resume();
	}
}

void ImageViewer::WaitAllThreads()
{
	for (int i = 0; i < THREAD_NUM; i++)
	{
		m_Threads[i]->WaitSync();
	}
}
