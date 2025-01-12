/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** H264_V4l2DeviceSource.cpp
** 
** H264 V4L2 Live555 source 
**
** -------------------------------------------------------------------------*/

#include <sstream>

// live555
#include <Base64.hh>

// project
#include "H264_V4l2DeviceSource.h"

#ifdef DEBUG
#define LOG_DEBUG(X) std::cout << X << std::endl
#else
#define LOG_DEBUG(X)
#endif


// ---------------------------------
// H264 V4L2 FramedSource
// ---------------------------------


// split packet in frames					
std::list< std::pair<unsigned char*,size_t> > H264_V4L2DeviceSource::splitFrames(unsigned char* frame, unsigned frameSize) 
{				
	std::list< std::pair<unsigned char*,size_t> > frameList;
	
	size_t bufSize = frameSize;
	size_t size = 0;
	unsigned char* buffer = this->extractFrame(frame, bufSize, size);
	while (buffer != NULL)
	{	
		switch (m_frameType&0x1F)
		{
			case 7: LOG_DEBUG("SPS size:" << size << " bufSize:" << bufSize); m_sps.assign((char*)buffer,size); break;
			case 8: LOG_DEBUG("PPS size:" << size << " bufSize:" << bufSize); m_pps.assign((char*)buffer,size); break;
			case 5: LOG_DEBUG("IDR size:" << size << " bufSize:" << bufSize); 
				if (m_repeatConfig && !m_sps.empty() && !m_pps.empty())
				{
					frameList.push_back(std::pair<unsigned char*,size_t>((unsigned char*)m_sps.c_str(), m_sps.size()));
					frameList.push_back(std::pair<unsigned char*,size_t>((unsigned char*)m_pps.c_str(), m_pps.size()));
				}
			break;
			default: 
				break;
		}
		
		if (m_auxLine.empty() && !m_sps.empty() && !m_pps.empty())
		{
			u_int32_t profile_level_id = 0;
			if (m_sps.size() >= 4) profile_level_id = (m_sps[1]<<16)|(m_sps[2]<<8)|m_sps[3]; 
		
			char* sps_base64 = base64Encode(m_sps.c_str(), m_sps.size());
			char* pps_base64 = base64Encode(m_pps.c_str(), m_pps.size());

			std::ostringstream os; 
			os << "profile-level-id=" << std::hex << std::setw(6) << std::setfill('0') << profile_level_id;
			os << ";sprop-parameter-sets=" << sps_base64 <<"," << pps_base64;
			m_auxLine.assign(os.str());
			std::cout << m_auxLine << std::endl;
			
			delete [] sps_base64;
			delete [] pps_base64;
		}
		frameList.push_back(std::pair<unsigned char*,size_t>(buffer, size));
		
		buffer = this->extractFrame(&buffer[size], bufSize, size);
	}
	return frameList;
}

// extract a frame
unsigned char*  H26X_V4L2DeviceSource::extractFrame(unsigned char* frame, size_t& size, size_t& outsize)
{
	unsigned char * outFrame = NULL;
	outsize = 0;
	unsigned int markerlength = 0;
	m_frameType = 0;
	
	unsigned char *startFrame = (unsigned char*)memmem(frame,size,H264marker,sizeof(H264marker));
	if (startFrame != NULL) {
		markerlength = sizeof(H264marker);
	} else {
		startFrame = (unsigned char*)memmem(frame,size,H264shortmarker,sizeof(H264shortmarker));
		if (startFrame != NULL) {
			markerlength = sizeof(H264shortmarker);
		}
	}
	if (startFrame != NULL) {
		m_frameType = startFrame[markerlength];
		
		int remainingSize = size-(startFrame-frame+markerlength);
		unsigned char *endFrame = (unsigned char*)memmem(&startFrame[markerlength], remainingSize, H264marker, sizeof(H264marker));
		if (endFrame == NULL) {
			endFrame = (unsigned char*)memmem(&startFrame[markerlength], remainingSize, H264shortmarker, sizeof(H264shortmarker));
		}
		
		if (m_keepMarker)
		{
			size -=  startFrame-frame;
			outFrame = startFrame;
		}
		else
		{
			size -=  startFrame-frame+markerlength;
			outFrame = &startFrame[markerlength];
		}
		
		if (endFrame != NULL)
		{
			outsize = endFrame - outFrame;
		}
		else
		{
			outsize = size;
		}
		size -= outsize;		
	} else if (size>= sizeof(H264shortmarker)) {
		 LOG_DEBUG("No marker found");
	}

	return outFrame;
}

