#pragma once

#include "stdafx.h"
#include <ImfArray.h>
#include <ImfThreading.h>
#include <ImfMultiPartInputFile.h>
#include <ImathLimits.h>
#include <ImfHeader.h>

#include <halfFunction.h>
#include <ImathFun.h>

#include <ImfStandardAttributes.h>
//#include <ImfHeader.h>

#include "loadImage.h"
#include "scaleImage.h"
#include "namespaceAlias.h"
#include "winsync.h"

#define max(a,b)            (((a) > (b)) ? (a) : (b))
//using namespace IMF;
//using namespace IMATH;
//using namespace std;
struct DispRect
{
	int x, y, w, h;
};

struct EXR_IMAGE
{
	IMF::Header hdr;
	IMF::Array<IMF::Rgba>		pixels;
	IMF::Array<float*>			dataZ;
	IMF::Array<unsigned int>	sampleCount;
	bool deepComp;
	int PartNum;
	int width;
	int height;
	int disp_w;
	int disp_h;
	int dx;
	int dy;
	float ratio;
	void *rgb;

	int Scale()
	{
		if (ratio > 1)
			scaleX(ratio, disp_w, disp_h, width, height, dx, dy, pixels);
		else
			scaleY(1 / ratio, disp_w, disp_h, width, height, dx, dy, pixels);
	}

	int Normalize()
	{
		normalizePixels(width, height, pixels);
	}

	int Mirror()
	{
		swapPixels(width, height, pixels);
	}

};

#define WARNING(message) (cerr << "Warning: " << message << endl)

inline IMF::Chromaticities displayChromaticities()
{
	//
	// Get the chromaticities of the display's primaries and
	// white point from an environment variable.  If this fails,
	// assume chromaticities according to Rec. ITU-R BT.709.
	//

	static const char chromaticitiesEnv[] = "CTL_DISPLAY_CHROMATICITIES";
	IMF::Chromaticities c;  // default-initialized according to Rec. 709

	if (const char *chromaticities = getenv(chromaticitiesEnv))
	{
		IMF::Chromaticities tmp;

		int n = sscanf(chromaticities,
			" red %f %f green %f %f blue %f %f white %f %f",
			&tmp.red.x, &tmp.red.y,
			&tmp.green.x, &tmp.green.y,
			&tmp.blue.x, &tmp.blue.y,
			&tmp.white.x, &tmp.white.y);

		if (n == 8)
			c = tmp;
		else
			printf("Cannot parse environment variable , using default value "
				"(chromaticities according to Rec. ITU-R BT.709).");
	}
	else
	{
		printf("Environment variable is not set; using default value (chromaticities according "
			"to Rec. ITU-R BT.709).");
	}

	return c;
}

inline void adjustChromaticities(const IMF::Header &header,
	const IMF::Array<IMF::Rgba> &inPixels,
	int w, int h,
	IMF::Array<IMF::Rgba> &outPixels)
{
	IMF::Chromaticities fileChroma;  // default-initialized according to Rec. 709

	if (hasChromaticities(header))
		fileChroma = chromaticities(header);

	IMF::Chromaticities displayChroma = displayChromaticities();

	if (fileChroma.red == displayChroma.red &&
		fileChroma.green == displayChroma.green &&
		fileChroma.blue == displayChroma.blue &&
		fileChroma.white == displayChroma.white)
	{
		return;
	}

	IMATH::M44f M = RGBtoXYZ(fileChroma, 1) * XYZtoRGB(displayChroma, 1);

	size_t numPixels = w * h;

	for (size_t i = 0; i < numPixels; ++i)
	{
		const IMF::Rgba &in = inPixels[i];
		IMF::Rgba &out = outPixels[i];

		IMATH::V3f rgb = IMATH::V3f(in.r, in.g, in.b) * M;

		out.r = rgb[0];
		out.g = rgb[1];
		out.b = rgb[2];
	}
}

inline void GetSize(IMF::Header* hdr, int& width, int& height, int& disp_w, int& disp_h, int& dx, int& dy, float& ratio)
{
	IMATH_NAMESPACE::Box2i drect = hdr->dataWindow();
	IMATH_NAMESPACE::Box2i disp = hdr->displayWindow();
	width  = drect.max.x - drect.min.x + 1;
	height = drect.max.y - drect.min.y + 1;

	ratio = hdr->pixelAspectRatio();
	dx = drect.min.x - disp.min.x;
	dy = drect.min.y - disp.min.y;
	disp_w = disp.max.x - disp.min.x + 1;
	disp_h = disp.max.y - disp.min.y + 1;
}

inline void LoadExrImage(
	const char fileName[],
	const char channel[],
	const char layer[],
	bool preview,
	int lx,
	int ly,
	int PartIndex,
	int &zsize, 
	EXR_IMAGE* image)
{
	if (image)
	{
		loadImage(fileName, channel, layer, preview, lx, ly, PartIndex, zsize, image->hdr, image->pixels, image->dataZ, image->sampleCount, image->deepComp);
		GetSize(&image->hdr, image->width, image->height, image->disp_w, image->disp_h, image->dx, image->dy, image->ratio);
	}
}

inline float
Knee(double x, double f)
{
	return float(IMATH::Math<double>::log(x * f + 1) / f);
}


inline float
FindKneeF(float x, float y)
{
	float f0 = 0;
	float f1 = 1;

	while (Knee(x, f1) > y)
	{
		f0 = f1;
		f1 = f1 * 2;
	}

	for (int i = 0; i < 30; ++i)
	{
		float f2 = (f0 + f1) / 2;
		float y2 = Knee(x, f2);

		if (y2 < y)
			f1 = f2;
		else
			f0 = f2;
	}

	return (f0 + f1) / 2;
}

struct Gamma
{
	float g, m, d, kl, f, s;

	Gamma(float gamma,
		float exposure,
		float defog,
		float kneeLow,
		float kneeHigh)	:g(gamma),
		m(IMATH::Math<float>::pow(2, exposure + 2.47393)),
		d(defog),
		kl(IMATH::Math<float>::pow(2, kneeLow)),
		f(FindKneeF(IMATH::Math<float>::pow(2, kneeHigh) - kl,
			IMATH::Math<float>::pow(2, 3.5) - kl)),
		s(255.0 * IMATH::Math<float>::pow(2, -3.5 * g))
	{}

	float operator () (half h)
	{
		//
		// Defog
		//

		float x = max(0.f, (h - d));

		//
		// Exposure
		//

		x *= m;

		//
		// Knee
		//

		if (x > kl)
			x = kl + Knee(x - kl, f);

		//
		// Gamma
		//

		x = IMATH::Math<float>::pow(x, g);

		//
		// Scale and clamp
		//

		return IMATH_NAMESPACE::clamp(x * s, 0.f, 255.f);
	}

};

class ImageViewer
{
#define THREAD_NUM	4
public:
	ImageViewer(HWND wnd) : m_hWnd(wnd), m_WndWidth(0), m_WndHeight(0),
		m_bStretchDisplay(false),
	m_gamma(1.0 / 2.2), m_exposure(-1), m_defog(0),
	m_kneeLow(0),
	m_kneeHigh(0),
	m_fogR(0),
	m_fogG(0),
	m_fogB(0)
	{
		//ZeroMemory(&m_img, sizeof(m_img));
		m_img.rgb = nullptr;
		m_brush = CreateSolidBrush(RGB(255, 255, 255));
		ZeroMemory(&m_StretchRect, sizeof(DispRect));
		ZeroMemory(&m_ScrollRect, sizeof(DispRect));
		CreateThreads(false);
	}
	virtual ~ImageViewer()
	{
		CloseImage();
		DeleteObject(m_brush);
		DestroyThreads();
	}

	virtual inline void SetExposure(float v) { m_exposure = v; }
	virtual inline void SetDefog(float v) { m_defog = v; }
	virtual inline void SetKneeLow(float v) { m_kneeLow = v; }
	virtual inline void SetKneeHigh(float v) { m_kneeHigh = v; }

	void OpenImage(const char * filename)
	{
		//	CreateThreads();
#if 1
		if (!_stricmp(filename, m_szFileName))
			return;

		strcpy_s(m_szFileName, MAX_PATH, filename);

		if (m_img.pixels)
		{
			ClearImage();
			CloseImage();
		}
#endif
		//int partIdx = 0;
		LoadEXR(filename, 0, 0, false, -1, -1/*, partIdx*/);

		m_img.rgb = new BYTE[m_img.width * m_img.height * 3];

		UpdateImage();
		DrawImage();
	}

	void UpdateImage()
	{
		if (!m_img.rgb)
			return;
		ComputeFogColor(m_img.pixels, m_img.width, m_img.height);

		// multi-thread
#if 1
		for (int i = 0; i < THREAD_NUM; i++)
		{
			SetEvent(m_hResumeEvts[i]);
			//m_threads[i].Resume();
		}

		WaitForMultipleObjects(THREAD_NUM, /*handles*/m_hevents, TRUE, INFINITE);
		for (int i = 0; i < THREAD_NUM; i++)
		{
			//m_threads[i].Suspend();
		}
		//for (int i = 0; i < THREAD_NUM; i++)
		//{
		//	WaitForSingleObject(m_hevents[i], INFINITE);
		//	DestroyThreads();
		//}
#else
		{
			CreateThreads();
			WaitForMultipleObjects(THREAD_NUM, /*handles*/m_hevents, TRUE, INFINITE);
			DestroyThreads();
		}
#endif
	}
	void InvalidateRect()
	{
		::InvalidateRect(m_hWnd, nullptr, true);
	}
	DispRect* GetScrollRect() { return &m_ScrollRect; }
	bool UpdateWndRect()
	{
		bool updated = false;
		RECT rc;
		::GetClientRect(m_hWnd, &rc);
		int ww = rc.right - rc.left;
		int wh = rc.bottom - rc.top;

		if (ww != m_WndWidth)
		{
			m_WndWidth = ww;
			updated = true;
		}
		if (wh != m_WndHeight)
		{
			m_WndHeight = wh;
			updated = true;
		}
		if (updated)
		{
			if (m_bStretchDisplay)
			{
				float r_wnd = (float)m_WndWidth / (float)m_WndHeight;
				float r_img = (float)m_img.width / (float)m_img.height;
				if (r_wnd >= r_img)
				{
					m_StretchRect.h = m_WndHeight;
					m_StretchRect.w = m_WndHeight * r_img;
					m_StretchRect.x = (m_WndWidth - m_StretchRect.w) / 2;
					m_StretchRect.y = 0;
				}
				else
				{
					m_StretchRect.h = (int)((float)m_WndWidth / r_img);
					m_StretchRect.w = m_WndWidth;
					m_StretchRect.x = 0;
					m_StretchRect.y = (m_WndHeight - m_StretchRect.h) / 2;
				}
			}
			else
			{
				// scroll display
				m_ScrollRect.w = m_WndWidth;
				m_ScrollRect.h = m_WndHeight;
			}
		}
		return updated;
	}

	void DrawImage()
	{
		if (!m_img.rgb)
			return;

		//WaitForMultipleObjects(THREAD_NUM, /*handles*/m_hevents, TRUE, INFINITE);

		HDC hdc = GetDC(m_hWnd);
		HDC hMemDC = CreateCompatibleDC(hdc);
		HBITMAP hBmp = CreateCompatibleBitmap(hdc, m_img.width, m_img.height);
		SelectObject(hMemDC, (HGDIOBJ)hBmp);

		BITMAPINFO bmpInfo;
		bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmpInfo.bmiHeader.biWidth = m_img.width;
		bmpInfo.bmiHeader.biHeight = -m_img.height;
		bmpInfo.bmiHeader.biPlanes = 1;
		bmpInfo.bmiHeader.biBitCount = 24;
		bmpInfo.bmiHeader.biCompression = BI_RGB;

		SetDIBits(hMemDC, hBmp, 0, abs(bmpInfo.bmiHeader.biHeight), (BYTE *)m_img.rgb, &bmpInfo, DIB_RGB_COLORS);

		UpdateWndRect();
		if (m_bStretchDisplay)
		{
			SetStretchBltMode(hdc, COLORONCOLOR);
			StretchBlt(hdc, m_StretchRect.x, m_StretchRect.y, m_StretchRect.w, m_StretchRect.h,
				hMemDC, 0, 0, bmpInfo.bmiHeader.biWidth, abs(bmpInfo.bmiHeader.biHeight), SRCCOPY);
		}
		else
		{
			BitBlt(hdc, 0, 0, bmpInfo.bmiHeader.biWidth, abs(bmpInfo.bmiHeader.biHeight), 
				hMemDC, m_ScrollRect.x, m_ScrollRect.y, SRCCOPY);
		}
		DeleteDC(hMemDC);
		DeleteObject(hBmp);
		ReleaseDC(m_hWnd, hdc);
	}

	void ClearImage()
	{
		// clear();
		HDC hdc = GetDC(m_hWnd);
		RECT rc;
		GetClientRect(m_hWnd, &rc);
		FillRect(hdc, &rc, m_brush);
		ReleaseDC(m_hWnd, hdc);
	}

	void CloseImage()
	{
		SAFEDELETE(m_img.rgb);
	}

	bool m_bthread_inited = false;

	static DWORD WINAPI ThreadProc(LPVOID param)
	{
		THREAD_CTXT* ctxt = (THREAD_CTXT *)param;
		int index = ctxt->index;
		ImageViewer* v = (ImageViewer*)ctxt->arg;
		v->ThreadCb(index);
		return 0;
	}
	bool m_bExit = false;

	DWORD ThreadCb(int index)
	{
		while (!m_bExit)
		{
			WaitForSingleObject(m_hResumeEvts[index], INFINITE);
			halfFunction<float>
				rGamma(Gamma(m_gamma,
					m_exposure,
					m_defog * m_fogR,
					m_kneeLow,
					m_kneeHigh),
					-HALF_MAX, HALF_MAX,
					0.f, 255.f, 0.f, 0.f);

			halfFunction<float>
				gGamma(Gamma(m_gamma,
					m_exposure,
					m_defog * m_fogG,
					m_kneeLow,
					m_kneeHigh),
					-HALF_MAX, HALF_MAX,
					0.f, 255.f, 0.f, 0.f);

			halfFunction<float>
				bGamma(Gamma(m_gamma,
					m_exposure,
					m_defog * m_fogB,
					m_kneeLow,
					m_kneeHigh),
					-HALF_MAX, HALF_MAX,
					0.f, 255.f, 0.f, 0.f);
#if 0
			int x_index = index & 0x1;
			int y_index = (index >> 1) & 0x1;
			int ww = img.width / 2;
			int hh = img.height / 2;
			int x_start = x_index * ww;
			int y_start = y_index * hh;
			int x_end = x_start + ww;
			int y_end = y_start + hh;
#else
			int x_index = 0;
			int y_index = (index);
			int ww = m_img.width;
			int hh = m_img.height / THREAD_NUM;
			int x_start = x_index * ww;
			int y_start = y_index * hh;
			int x_end = x_start + ww;
			int y_end = y_start + hh;
#endif
			Imf::Rgba *pixel, *pixel0;

			BYTE *pRgbData00 = (BYTE*)m_img.rgb;
			BYTE *pRgbData0 = pRgbData00;
			BYTE *pRgbData = pRgbData0;

			for (int y = y_start; y < y_end && y < m_img.height; y++)
			{
				pixel0 = &(m_img.pixels[m_img.width * y]);
				pRgbData0 = pRgbData00 + m_img.width * y * 3;
				for (int x = x_start; x < x_end; x++)
				{
					pixel = pixel0 + x;
					pRgbData = pRgbData0 + x * 3;
					*pRgbData++ = (unsigned char)(dither(bGamma(pixel->b), x, y) /** 255.f*/);
					*pRgbData++ = (unsigned char)(dither(gGamma(pixel->g), x, y) /** 255.f*/);
					*pRgbData++ = (unsigned char)(dither(rGamma(pixel->r), x, y) /** 255.f*/);
				}
			}
			SetEvent(m_hevents[index]);
			//m_threads[index].Suspend();
		}
		return 0;
	}

	void CreateThreads(bool bsuspend= false)
	{
		//if (m_bthread_inited)
		//	return;
		m_threads = new WinThread[THREAD_NUM];
		for (int i = 0; i < THREAD_NUM; i++)
		{
			m_hResumeEvts[i] = CreateEventW(nullptr, false/*auto - reset*/, false, nullptr);
			m_hevents[i] = CreateEventW(nullptr, false/*auto - reset*/, false, nullptr);
			m_handles[i] = m_threads[i].GetHandle();
			m_ctxt[i].index = i;
			m_ctxt[i].arg = this;

			m_threads[i].Create(ThreadProc, &(m_ctxt[i]), bsuspend);
		}
		m_bthread_inited = true;
	}

	void DestroyThreads()
	{
		m_bExit = true;
		WaitForMultipleObjects(THREAD_NUM, m_handles, TRUE, INFINITE);

		m_bthread_inited = false;
		SAFEDELETEARRAY(m_threads);
		for (int i = 0; i < THREAD_NUM; i++)
		{
			CloseHandle(m_hevents[i]);
			CloseHandle(m_hResumeEvts[i]);
		}
	}


protected:

	virtual int GetPartNum(const char* filename)
	{
		int numparts = 0;

		try
		{
			IMF::MultiPartInputFile *infile = new IMF::MultiPartInputFile(filename);
			numparts = infile->parts();
			delete infile;
		}
		catch (IEX_NAMESPACE::BaseExc &e)
		{
			printf("error: %s", e.what());
			return 0;
		}
		return numparts;
	}
	virtual void LoadEXR(
		const char fileName[],
		const char channel[],
		const char layer[],
		bool preview,
		int lx,
		int ly/*,
		int PartIndex0*/)
	{
		int partNum = GetPartNum(fileName);
		int PartIndex = 0;
		LoadExrImage(fileName, channel, layer, preview, lx, ly, PartIndex, m_zsize, &m_img);
		GetSize(&(m_img.hdr), m_img.width, m_img.height, m_img.disp_w, m_img.disp_h, m_img.dx, m_img.dy, m_img.ratio);
		m_img.PartNum = partNum;
	}
	//
	//  Dithering: Reducing the raw 16-bit pixel data to 8 bits for the
	//  OpenGL frame buffer can sometimes lead to contouring in smooth
	//  color ramps.  Dithering with a simple Bayer pattern eliminates
	//  visible contouring.
	//
	unsigned char dither(float v, int x, int y)
	{
		static const float d[4][4] =
		{
			{ 0.f / 16,  8.f / 16,  2.f / 16, 10.f / 16 },
			{ 12.f / 16,  4.f / 16, 14.f / 16,  6.f / 16 },
			{ 3.f / 16, 11.f / 16,  1.f / 16,  9.f / 16 },
			{ 15.f / 16,  7.f / 16, 13.f / 16,  5.f / 16 }
		};

		return (unsigned char)(v + d[y & 3][x & 3]);
	}

	void ComputeFogColor(IMF::Rgba* rp, int _dw, int _dh)
	{
		m_fogR = 0;
		m_fogG = 0;
		m_fogB = 0;

		for (int j = 0; j < _dw * _dh; ++j)
		{
			//const IMF::Rgba &rp = _rawPixels[j];

			if (rp->r.isFinite())
				m_fogR += rp->r;

			if (rp->g.isFinite())
				m_fogG += rp->g;

			if (rp->b.isFinite())
				m_fogB += rp->b;
		}

		m_fogR /= _dw * _dh;
		m_fogG /= _dw * _dh;
		m_fogB /= _dw * _dh;
	}

	struct THREAD_CTXT
	{
		int index;
		void* arg;
	};

	WinThread* m_threads;
	HANDLE m_hResumeEvts[THREAD_NUM];
	HANDLE m_hevents[THREAD_NUM];
	HANDLE m_handles[THREAD_NUM];
	THREAD_CTXT m_ctxt[THREAD_NUM] = {};
	char m_szFileName[MAX_PATH];
	int m_zsize;
	EXR_IMAGE m_img;
	HWND m_hWnd;
	int m_WndWidth;
	int m_WndHeight;
	bool m_bStretchDisplay;
	DispRect m_StretchRect;
	DispRect m_ScrollRect;
	HBRUSH m_brush;

	float m_gamma;
	float m_exposure;
	float m_defog;
	float m_kneeLow;
	float m_kneeHigh;
	float m_fogR;
	float m_fogG;
	float m_fogB;
};