#pragma once
#include <malloc.h>
#include <memory>

class DigitalFilter
{
private:
	// coefficients and filter state
	//double* m_pdA;
	//double* m_pdB;
	//double* m_pdZ;
	std::vector<double> m_pdA;
	std::vector<double> m_pdB;
	std::vector<double> m_pdZ;
	// local state variables for direct form II
	double m_dXi;
	double m_dYi;

	// filter data
	int m_nFilterOrder;
	int m_nFilterBlockLen; // this is how many samples to process at a time

	bool m_bIsInitialized;
public:

	DigitalFilter()
	{
		m_bIsInitialized = false;
	}

	DigitalFilter(const DigitalFilter& obj)
	{
		if (obj.m_bIsInitialized)
		{
			m_nFilterOrder = obj.m_nFilterOrder;
			m_nFilterBlockLen = obj.m_nFilterBlockLen;
			m_pdA = obj.m_pdA;
			m_pdB = obj.m_pdB;
			m_pdZ = obj.m_pdZ;
			m_dXi = obj.m_dXi;
			m_dYi = obj.m_dYi;
			m_bIsInitialized = true;
		}
		else
			m_bIsInitialized = false;
	}

	template<class T>
	void Filter(T* ptIn, T* ptOut)
	{
		int i, j;
		for (i = 0; i < m_nFilterBlockLen; i++)
		{
			m_dXi = (double)ptIn[i];
			m_dYi = m_pdB[0] * m_dXi + m_pdZ[0];
			for (j = 1; j < m_nFilterOrder + 1; j++)
				m_pdZ[j - 1] = m_pdB[j] * m_dXi + m_pdZ[j] - m_pdA[j] * m_dYi;
			m_pdZ[m_nFilterOrder - 1] = m_pdB[m_nFilterOrder] * m_dXi - m_pdA[m_nFilterOrder] * m_dYi;
			ptOut[i] = (T)(m_pdA[0] * m_dYi);
		}
	}

	void Init(int nOrder, int nBlockLen, double* pdB, double* pdA, double* pdZ)
	{
		m_nFilterOrder = nOrder;
		m_nFilterBlockLen = nBlockLen;

		bool hasA = (pdA != NULL) ? true : false;
		bool hasZ = (pdZ != NULL) ? true : false;

		m_pdA.reserve(m_nFilterOrder + 1);
		m_pdB.reserve(m_nFilterOrder + 1);
		m_pdZ.reserve(m_nFilterOrder + 1);
		m_pdA.resize(m_nFilterOrder + 1, 0);
		m_pdB.resize(m_nFilterOrder + 1, 0);
		m_pdZ.resize(m_nFilterOrder + 1, 0);

		int i;
		for (i = 0; i < m_nFilterOrder; i++)
		{
			m_pdB[i] = pdB[i];
			hasA ? m_pdA[i] = pdA[i] : m_pdA[i] = 0.0;
			hasZ ? m_pdZ[i] = pdZ[i] : m_pdZ[i] = 0.0;
		}
		!hasA ? m_pdA[0] = 1 : 1 - 2; // if a coefficients are provided, do nothing (1-2) 

		m_pdB[m_nFilterOrder] = pdB[m_nFilterOrder];
		hasA ? m_pdA[m_nFilterOrder] = pdA[m_nFilterOrder] : m_pdA[m_nFilterOrder] = 0;
		m_bIsInitialized = true;
	}

	~DigitalFilter()
	{

	}
};

const double pdBCoeffs2[] = { 0.914969144113082, 1.829938288226165,   0.914969144113082 };
const double pdACoeffs2[] = { 1.000000000000000,   1.822694925196307,   0.837181651256022 };
const double pdBCoeffs5[] = { 0.800592403464570,   1.601184806929141,   0.800592403464570};
const double pdACoeffs5[] = { 1.000000000000000,   1.561018075800718,   0.641351538057563 };
const double pdBCoeffs10[] = { 0.800592403464570,   1.601184806929141,   0.800592403464570 };
const double pdACoeffs10[] = { 1.000000000000000,   1.561018075800718,   0.641351538057563 };
const double pdBCoeffsElse[] = { 0.800592403464570,   1.601184806929141,   0.800592403464570 };
const double pdACoeffsElse[] = { 1.000000000000000,   1.561018075800718,   0.641351538057563 };
//const double pdBCoeffs25[] = { 0.003621681514929,   0.007243363029857,   0.003621681514929};
//const double pdACoeffs25[] = { 1.000000000000000, -1.822694925196308,   0.837181651256023};
//const double pdBCoeffs50[] = { 0.020083365564211,   0.040166731128423,   0.020083365564211};
//const double pdACoeffs50[] = { 1.000000000000000, - 1.561018075800718,   0.641351538057563};
template<class T>
class Downsampler
{
private:
	std::shared_ptr<DigitalFilter> m_pDigitalFilter;
	int m_nDownsamplingFactor;
	int m_nChunkLen;
	bool m_bFilterSignal;
	std::vector<T> m_ptFilteredSignal;
	//T* m_ptFilteredSignal = NULL;
	void SetupMemory()
	{
		m_ptFilteredSignal.reserve(m_nChunkLen * m_nDownsamplingFactor);
		m_ptDataOut.reserve(m_nChunkLen);
		m_ptFilteredSignal.resize(m_nChunkLen * m_nDownsamplingFactor, 0);
		m_ptDataOut.resize(m_nChunkLen,0);

	}
public:
	
	std::vector<T> m_ptDataOut;
	Downsampler() {}
	Downsampler(int nDownsamplingFactor, int nChunkLen, bool bFilterSignal = true)
	{
		m_nDownsamplingFactor = nDownsamplingFactor;
		m_nChunkLen = nChunkLen;
		m_bFilterSignal = bFilterSignal;
		double pdBCoeffs[3];
		double pdACoeffs[3];
		switch (nDownsamplingFactor)
		{
		case 2:
			for (int i = 0; i < 3; i++)
			{
				pdBCoeffs[i] = pdBCoeffs2[i];
				pdACoeffs[i] = pdACoeffs2[i];
			}
			break;
		case 5:
			for (int i = 0; i < 3; i++)
			{
				pdBCoeffs[i] = pdBCoeffs5[i];
				pdACoeffs[i] = pdACoeffs5[i];
			}
			break;

		case 10:
			for (int i = 0; i < 3; i++)
			{
				pdBCoeffs[i] = pdBCoeffs10[i];
				pdACoeffs[i] = pdACoeffs10[i];
			}
			break;
		case 20:
		case 25:
		case 50:
			for (int i = 0; i < 3; i++)
			{
				pdBCoeffs[i] = pdBCoeffsElse[i];
				pdACoeffs[i] = pdACoeffsElse[i];
			}
			break;
		//case 25:
		//	for (int i = 0; i < 3; i++)
		//	{
		//		pdBCoeffs[i] = pdBCoeffs25[i];
		//		pdACoeffs[i] = pdACoeffs25[i];
		//	}
		//	break;
		//case 50:
		//	for (int i = 0; i < 3; i++)
		//	{
		//		pdBCoeffs[i] = pdBCoeffs50[i];
		//		pdACoeffs[i] = pdACoeffs50[i];
		//	}
		//	break;
		
		}
	
		m_pDigitalFilter.reset(new DigitalFilter());
		m_pDigitalFilter->Init(2, nChunkLen * nDownsamplingFactor, pdBCoeffs, pdACoeffs, NULL);

		SetupMemory();
	}

	Downsampler(const Downsampler& obj)
	{
		m_nDownsamplingFactor = obj.m_nDownsamplingFactor;
		m_nChunkLen = obj.m_nChunkLen;
		m_bFilterSignal = obj.m_bFilterSignal;
		m_ptDataOut = obj.m_ptDataOut;
		m_ptFilteredSignal = obj.m_ptFilteredSignal;
		m_pDigitalFilter.reset(new DigitalFilter(*(obj.m_pDigitalFilter.get())));
		SetupMemory();
	}
	void Downsample(T* ptDataIn)
	{
		size_t sz = sizeof(T);
		if (m_bFilterSignal)
		{
			m_ptDataOut.clear();
			m_pDigitalFilter->Filter(ptDataIn, &m_ptFilteredSignal[0]);
			for (int i = 0; i < m_nChunkLen; i++)
				m_ptDataOut.push_back(m_ptFilteredSignal[i * m_nDownsamplingFactor]);
		}
		else
			m_ptDataOut.assign(ptDataIn, ptDataIn+(m_nChunkLen+m_nDownsamplingFactor));
			//memcpy(m_ptDataOut, ptDataIn, sz * m_nChunkLen * m_nDownsamplingFactor);
		
	}
	~Downsampler()
	{
		//if (m_ptDataOut != NULL)
		//{
		//	free(m_ptDataOut);
		//	m_ptDataOut = NULL;
		//}
		//if (m_ptFilteredSignal != NULL)
		//{
		//	free(m_ptFilteredSignal);
		//	m_ptFilteredSignal = NULL;
		//}
	}
};