// ===========================================================
//     _/_/_/   _/_/_/  _/_/_/_/    _/_/_/_/  _/_/_/   _/_/_/
//      _/    _/       _/             _/    _/    _/   _/   _/
//     _/    _/       _/_/_/_/       _/    _/    _/   _/_/_/
//    _/    _/       _/             _/    _/    _/   _/
// _/_/_/   _/_/_/  _/_/_/_/_/     _/     _/_/_/   _/_/
// ===========================================================
//
// ConvVCF2GDS.cpp: the C++ code for the conversion from VCF to GDS
//
// Copyright (C) 2013 - 2014	Xiuwen Zheng [zhengxwen@gmail.com]
//
// This file is part of SeqArray.
//
// SeqArray is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 3 as
// published by the Free Software Foundation.
//
// SeqArray is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SeqArray.
// If not, see <http://www.gnu.org/licenses/>.

#include <R_GDS_CPP.h>
#include <string>
#include <vector>
#include <algorithm>


using namespace std;
using namespace CoreArray;



/// check CoreArray function
COREARRAY_INLINE static const char *SKIP(const char *p)
{
	while (isspace(*p)) p ++;
	return p;
}

/// check CoreArray function
COREARRAY_INLINE static string SHORT_TEXT(const char *p, int MaxNum=16)
{
	if ((int)strlen(p) <= MaxNum)
		return string(p);
	else
		return string(p, MaxNum) + "...";
}


// ###########################################################
// define 
// ###########################################################

static const string BlackString;


// ###########################################################
// the structure of read line
// ###########################################################

/// a class of parsing text
class CReadLine
{
public:
	/// constructor
	CReadLine()
	{
		_ReadFun = _Rho = R_NilValue;
		_ptr_line = _lines.end();
		_ifend = false; _line_no = _column_no = 0;
		_cur_char = NULL;
		nProt = 0;
		SplitBySpaceTab();
	}
	/// constructor
	CReadLine(SEXP vFun, SEXP vRho)
	{
		Init(vFun, vRho);
		SplitBySpaceTab();
	}

	~CReadLine()
	{
		if (nProt > 0)
			UNPROTECT(nProt);
	}

	/// initialize R call
	void Init(SEXP vFun, SEXP vRho)
	{
		_ReadFun = vFun; _Rho = vRho;	
		_lines.clear(); _ptr_line = _lines.end();
		_ifend = false; _line_no = _column_no = 0;
		_cur_char = NULL;
		nProt = 0;
	}

	/// read a new line
	const char *ReadLine()
	{
		if (_ifend) return NULL;
		_cur_char = NULL;
		if (_ptr_line == _lines.end())
		{
			if (_PrepareBuffer())
			{
				const char *rv = *_ptr_line;
				_ptr_line ++; _line_no ++;
				return rv;
			} else
				return NULL;
		} else {
			const char *rv = *_ptr_line;
			_ptr_line ++; _line_no ++;
			return rv;
		}
	}

	/// get a string with a seperator '\t'
	void GetCell(string &buffer, bool last_column)
	{
		if (_ifend)
			throw ErrCoreArray("It is the end.");
		if (!_cur_char)
		{
			_cur_char = ReadLine();
			if (!_cur_char)
				throw ErrCoreArray("It is the end.");
			_column_no = 0;
		}

		const char *str_begin = _cur_char;
		while ((_sepTab ? (*_cur_char != '\t') : true) &&
			(_sepSpace ? (*_cur_char != ' ') : true) && (*_cur_char != 0))
		{
			_cur_char ++;
		}
		const char *str_end = _cur_char;
		_column_no ++;

		// check
		if ((str_begin == str_end) && (*_cur_char == 0))
			throw ErrCoreArray("fewer columns than what expected.");
		if (last_column)
		{
			while (*_cur_char == ' ')
				_cur_char ++;
			if (*_cur_char != 0)
				throw ErrCoreArray("more columns than what expected.");
			_cur_char = NULL;
		} else {
			// = '\t', ' ' or '\x0'
			if (*_cur_char == '\t')
				_cur_char ++;
			else if (*_cur_char == ' ')
			{
				while (*_cur_char == ' ')
					_cur_char ++;
			}
		}
		buffer.assign(str_begin, str_end);
	}

	/// skip the current line
	void SkipLine()
	{
		_cur_char = ReadLine();
		_column_no = 0;
	}

	/// return true, if it is of the end
	bool IfEnd()
	{
		if (!_ifend)
		{
			if (_ptr_line == _lines.end())
				_PrepareBuffer();
		}
		return _ifend;
	}

	/// split by tabs
	void SplitByTab()
	{
		_sepSpace = false; _sepTab = true;
	}
	
	/// split by tabs and spaces
	void SplitBySpaceTab()
	{
		_sepSpace = _sepTab = true;
	}

	/// return line number
	COREARRAY_INLINE int LineNo() { return _line_no; }
	/// return column number
	COREARRAY_INLINE int ColumnNo() { return _column_no; }
	///
	COREARRAY_INLINE void ClearColumnNo() { _column_no = 0; }

protected:
	SEXP _ReadFun;  //< R call function
	SEXP _Rho;      //< R environment
	vector<const char *> _lines;               //< store returned string(s)
	vector<const char *>::iterator _ptr_line;  //< the pointer to _lines
	bool _ifend;     //< true for the end of reading
	int _line_no;    //< the index of current line
	int _column_no;  //< the index of current column
	const char *_cur_char;  //< 
	int nProt;
	bool _sepSpace, _sepTab;

	bool _PrepareBuffer()
	{
		if (nProt > 0)
		{
			UNPROTECT(nProt);
			nProt = 0;
		}

		// call ReadLine R function
		SEXP val = eval(_ReadFun, _Rho);
		PROTECT(val);
		nProt ++;

		// check the returned value
		int n = Rf_length(val);
		if (n > 0)
		{
			_ifend = false;
			_lines.resize(n);
			for (int i=0; i < n; i++)
				_lines[i] = CHAR(STRING_ELT(val, i));
			_ptr_line = _lines.begin();
			return true;
		} else {
			_ifend = true;
			return false;
		}
	}
};




// ###########################################################
// VCF strcture
// ###########################################################

const static int FIELD_TYPE_INT      = 1;
const static int FIELD_TYPE_FLOAT    = 2;
// const static int FIELD_TYPE_FLAG     = 3;
const static int FIELD_TYPE_STRING   = 4;



/// the structure of FORMAT field
struct TVCF_Field_Format
{
	string name;           //< FORMAT ID
	int type;              //< 1: integer, 2: float, 3: flag, 4: character,
	bool import_flag;      //< true: import, false: not import
	PdSequenceX data_obj;  //< the pointer to data object
	PdSequenceX len_obj;   //< can be NULL if variable-length object
	int number;            //< according to 'Number' field, if -1: variable-length, -2: # of alleles, -3: # of genotypes
	bool used;             //< if TRUE, it has been parsed for the current line

	/// data -- Int32
	vector< vector<C_Int32> > I32ss;
	/// data -- Float32
	vector< vector<C_Float32> > F32ss;
	/// data -- UTF8 string
	vector< vector<string> > UTF8ss;


	TVCF_Field_Format() { type = 0; data_obj = len_obj = NULL; number = 0; used = false; }

	// FORMAT field

	template<typename TYPE>
		void Check(vector<TYPE> &array, string &name, int num_allele, const TYPE &missing)
	{
		switch (number)
		{
			case -1:
				break;

			case -2:
				// # of alleles
				if ((int)array.size() > (num_allele-1))
				{
					throw ErrCoreArray("FORMAT ID '%s' should have %d value(s).",
						name.c_str(), num_allele-1);
				} else {
					array.resize(num_allele-1, missing);
				}
				break;		

			case -3:
				// # of genotypes
				if ((int)array.size() > (num_allele+1)*num_allele/2)
				{
					throw ErrCoreArray("INFO ID '%s' should have %d value(s).",
						name.c_str(), (num_allele+1)*num_allele/2);
				} else {
					array.resize((num_allele+1)*num_allele/2, missing);
				}
				break;		

			default:
				if (number >= 0)
				{
					if ((int)array.size() > number)
					{
						throw ErrCoreArray("FORMAT ID '%s' should have %d value(s).",
							name.c_str(), number);
					} else {
						array.resize(number, missing);
					}
				} else
					throw ErrCoreArray("Invalid value 'number' in TVCF_Field_Format.");
		}
	}

	void WriteFixedLength()
	{
		if (number < 0)
			throw ErrCoreArray("Wrong call 'WriteFixedLength' in TVCF_Field_Format.");
		switch (type)
		{
			case FIELD_TYPE_INT:
				for (vector< vector<C_Int32> >::iterator it = I32ss.begin();
					it != I32ss.end(); it ++)
				{
					GDS_Seq_AppendData(data_obj, number, &((*it)[0]), svInt32);
				}
				break;

			case FIELD_TYPE_FLOAT:
				for (vector< vector<float> >::iterator it = F32ss.begin();
					it != F32ss.end(); it ++)
				{
					GDS_Seq_AppendData(data_obj, number, &((*it)[0]), svFloat32);
				}
				break;

			case FIELD_TYPE_STRING:
				for (vector< vector<string> >::iterator it = UTF8ss.begin();
					it != UTF8ss.end(); it ++)
				{
					for (int j=0; j < (int)(*it).size(); j ++)
						GDS_Seq_AppendString(data_obj, (*it)[j].c_str());
				}
				break;

			default:
				throw ErrCoreArray("Invalid FORMAT Type.");
		}
	}

	int WriteVariableLength(int nTotalSample, vector<C_Int32> &I32s,
		vector<C_Float32> &F32s)
	{
		if (number >= 0)
			throw ErrCoreArray("Wrong call 'WriteVariableLength' in TVCF_Field_Format.");

		int nMax = 0;
		switch (type)
		{
			case FIELD_TYPE_INT:
				for (int j=0; j < nTotalSample; j++)
				{
					if (nMax < (int)I32ss[j].size())
						nMax = I32ss[j].size();
				}
				I32s.resize(nTotalSample);
				for (int i=0; i < nMax; i++)
				{
					for (int j=0; j < nTotalSample; j++)
					{
						vector<C_Int32> &B = I32ss[j];
						I32s[j] = (i < (int)B.size()) ? B[i] : NA_INTEGER;
					}
					GDS_Seq_AppendData(data_obj, nTotalSample, &(I32s[0]), svInt32);
				}
				break;

			case FIELD_TYPE_FLOAT:
				for (int j=0; j < nTotalSample; j++)
				{
					if (nMax < (int)F32ss[j].size())
						nMax = F32ss[j].size();
				}
				F32s.resize(nTotalSample);
				for (int i=0; i < nMax; i++)
				{
					for (int j=0; j < nTotalSample; j++)
					{
						vector<float> &B = F32ss[j];
						F32s[j] = (i < (int)B.size()) ? B[i] : (float)R_NaN;
					}
					GDS_Seq_AppendData(data_obj, nTotalSample, &(F32s[0]), svFloat32);
				}
				break;

			case FIELD_TYPE_STRING:
				for (int j=0; j < nTotalSample; j++)
				{
					if (nMax < (int)UTF8ss[j].size())
						nMax = UTF8ss[j].size();
				}
				for (int i=0; i < nMax; i++)
				{
					for (int j=0; j < nTotalSample; j++)
					{
						vector<string> &B = UTF8ss[j];
						GDS_Seq_AppendString(data_obj,
							(i < (int)B.size()) ? B[i].c_str() : "");
					}
				}
				break;

			default:
				throw ErrCoreArray("Invalid FORMAT Type.");
		}
		return nMax;
	}
};


/// get an integer from a string
static C_Int32 getInt32(const string &txt, bool RaiseError)
{
	const char *p = SKIP(txt.c_str());
	char *endptr = (char*)p;
	long int val = strtol(p, &endptr, 10);

	if (endptr == p)
	{
		if ((*p != '.') && RaiseError)
		{
			throw ErrCoreArray("Invalid integer conversion \"%s\".",
				SHORT_TEXT(p).c_str());
		}
		val = NA_INTEGER;
	} else {
		if ((val < INT_MIN) || (val > INT_MAX))
		{
			val = NA_INTEGER;
			if (RaiseError)
			{
				throw ErrCoreArray("Invalid integer conversion \"%s\".",
					SHORT_TEXT(p).c_str());
			}
		}
		p = SKIP(endptr);
		if (*p != 0)
		{
			val = NA_INTEGER;
			if (RaiseError)
			{
				throw ErrCoreArray("Invalid integer conversion \"%s\".",
					SHORT_TEXT(p).c_str());
			}
		}
	}
	return val;
}

/// get a float number from a string
static double getFloat(string &txt, bool RaiseError)
{
	const char *p = SKIP(txt.c_str());
	char *endptr = (char*)p;
	double val = strtof(p, &endptr);
	if (endptr == p)
	{
		if ((*p != '.') && RaiseError)
		{
			throw ErrCoreArray("Invalid float conversion \"%s\".",
				SHORT_TEXT(p).c_str());
		}
		val = R_NaN;
	} else {
		p = SKIP(endptr);
		if (*p != 0)
		{
			val = R_NaN;
			if (RaiseError)
			{
				throw ErrCoreArray("Invalid float conversion \"%s\".",
					SHORT_TEXT(p).c_str());
			}
		}
	}
	return val;
}


extern "C"
{
// ###########################################################
// Convert from VCF4: VCF4 -> SNP GDS
// ###########################################################

static C_Int32 GDS_Variant_Index = 0;
static C_Int32 GDS_Global_Variant_Index = 0;

/// Initialize 'GDS_Variant_Index'
COREARRAY_DLL_EXPORT SEXP gnr_Init_Parse_VCF4()
{
	GDS_Variant_Index = 1;
	GDS_Global_Variant_Index = 0;
	return R_NilValue;
}


/** VCF4 --> SNP GDS
 *  \param vcf_fn            the file names of VCF format
 *  \param gds_root          the root of GDS file
 *  \param method            the method index: 1 -- biallelic.only
 *  \param ReadLineFun       the calling
 *  \param ReadLine_Param    the parameter for calling function
 *  \param RefAllele         the reference alleles
 *  \param rho               the environment
 *  \param Verbose           print out information
 *  \return                  the number of variants
**/
COREARRAY_DLL_EXPORT SEXP gnr_Parse_VCF4(SEXP vcf_fn, SEXP gds_root,
	SEXP method, SEXP ReadLineFun, SEXP ReadLine_Param, SEXP RefAllele,
	SEXP rho, SEXP Verbose)
{
	const char *fn = CHAR(STRING_ELT(vcf_fn, 0));
	int met_idx = INTEGER(method)[0];
	int verbose = asLogical(Verbose);
	if (verbose == NA_LOGICAL)
		error("'verbose' must be TRUE or FALSE.");

	// define a variable for reading lines
	CReadLine RL;

	// string buffer
	string cell, value;
	cell.reserve(4096);
	value.reserve(4096);

	COREARRAY_TRY

		// *********************************************************
		// initialize variables		

		const bool RaiseError = true;

		// GDS nodes
		PdGDSObj Root = GDS_R_SEXP2Obj(gds_root);
		PdSequenceX varIdx = GDS_Node_Path(Root, "snp.id", TRUE);
		PdSequenceX varRSID = GDS_Node_Path(Root, "snp.rs.id", TRUE);
		PdSequenceX varChr = GDS_Node_Path(Root, "snp.chromosome", TRUE);
		PdSequenceX varPos = GDS_Node_Path(Root, "snp.position", TRUE);
		PdSequenceX varAllele = GDS_Node_Path(Root, "snp.allele", TRUE);
		PdSequenceX varGeno = GDS_Node_Path(Root, "genotype", TRUE);
		PdSequenceX varQual = GDS_Node_Path(Root, "snp.annot/qual", TRUE);
		PdSequenceX varFilter = GDS_Node_Path(Root, "snp.annot/filter", TRUE);

		int nTotalSamp;
		{
			int D[2];
			GDS_Seq_GetDim(varGeno, D, 2);
			nTotalSamp = D[1];
		}

		// numeric buffer
		C_Int32 I32;
		C_Float32 F32;
		C_Int32 old_variant_index = GDS_Variant_Index;

		// genotype buffer
		vector<C_UInt8> U8s;
		U8s.resize(nTotalSamp);


		// *********************************************************
		// initialize external calling for reading stream

		SEXP R_Read_Call;
		PROTECT(R_Read_Call =
			LCONS(ReadLineFun, LCONS(ReadLine_Param, R_NilValue)));
		RL.Init(R_Read_Call, rho);
		RL.SplitByTab();

		// skip the header
		while (!RL.IfEnd())
		{
			const char *p = RL.ReadLine();
			if (strncmp(p, "#CHROM", 6) == 0)
				break;
		}


		// *********************************************************
		// parse the context

		string sCHROM, sPOS, sID, sREF, sALT;
		vector<string> AlleleList;
		R_xlen_t AlleleCount = XLENGTH(RefAllele);

		while (!RL.IfEnd())
		{
			GDS_Global_Variant_Index ++;

			// *****************************************************
			// scan line by line

			RL.GetCell(sCHROM, false);    // column 1: CHROM
			RL.GetCell(sPOS, false);      // column 2: POS
			RL.GetCell(sID, false);       // column 3: ID
			RL.GetCell(sREF, false);      // column 4: REF
			RL.GetCell(sALT, false);      // column 5: ALT

			if (met_idx == 1)
			{
				// biallelic.only
				bool flag =
					((sREF=="A" || sREF=="G" || sREF=="C" || sREF=="T" ||
					sREF=="a" || sREF=="g" || sREF=="c" || sREF=="t") &&
					(sALT=="A" || sALT=="G" || sALT=="C" || sALT=="T" ||
					sALT=="a" || sALT=="g" || sALT=="c" || sALT=="t"));
				if (!flag)
				{
					RL.SkipLine();
					continue;
				}
			}

			// the number of alleles in total at a specified site
			int num_allele = 1;
			if (sALT != ".")
			{
				const char *p = sALT.c_str();
				while (*p != 0)
				{
					num_allele ++;
					while ((*p != 0) && (*p != ',')) p ++;
					if (*p == ',') p ++;
				}
			}

			// #####################################################

			// variant id
			GDS_Seq_AppendData(varIdx, 1, &GDS_Variant_Index, svInt32);
			GDS_Variant_Index ++;

			// column 1: CHROM
			GDS_Seq_AppendString(varChr, sCHROM.c_str());

			// column 2: POS
			I32 = getInt32(sPOS, RaiseError);
			GDS_Seq_AppendData(varPos, 1, &I32, svInt32);

			// column 3: ID
			if (sID == ".") sID.clear();
			GDS_Seq_AppendString(varRSID, sID.c_str());

			// column 4 & 5: REF + ALT
			int ref_allele_index = 0;
			bool allele_flag = true;
			if (!Rf_isNull(RefAllele))
			{
				R_xlen_t I = GDS_Global_Variant_Index - 1;
				if (I >= AlleleCount)
				{
					RL.ClearColumnNo();
					throw ErrCoreArray(
						"'ref.allele' has fewer alleles than what expected.");
				}
				SEXP s = STRING_ELT(RefAllele, I);
				allele_flag = (s == NA_STRING);
			}
			if (allele_flag)
			{
				sREF.push_back('/');
				sREF.append(sALT);
			} else {
				AlleleList.clear();
				AlleleList.push_back(sREF);
				const char *s, *p = sALT.c_str();
				for (s=p; *s; s++)
				{
					if (*s == ',')
					{
						AlleleList.push_back(string(p, s));
						p = s+1;
					}
				}
				if (s != p)
					AlleleList.push_back(string(p, s));

				const char *ss = CHAR(STRING_ELT(RefAllele, GDS_Global_Variant_Index-1));
				ref_allele_index = -1;
				for (int i=0; i < (int)AlleleList.size(); i++)
				{
					if (AlleleList[i].compare(ss) == 0)
					{
						ref_allele_index = i;
						break;
					}
				}

				if (ref_allele_index >= 0)
				{
					sREF = AlleleList[ref_allele_index];
					sREF.append("/");
					AlleleList.erase(AlleleList.begin() + ref_allele_index);
					for (int i=0; i < (int)AlleleList.size(); i++)
					{
						if (i > 0) sREF.append(",");
						sREF.append(AlleleList[i]);
					}
				} else {
					sREF.insert(sREF.begin(), '/');
					sREF.insert(0, ss);
					sREF.append(",");
					sREF.append(sALT);
				}
			}
			GDS_Seq_AppendString(varAllele, sREF.c_str());

			// column 6: QUAL
			RL.GetCell(cell, false);
			F32 = getFloat(cell, RaiseError);
			GDS_Seq_AppendData(varQual, 1, &F32, svFloat32);

			// column 7: FILTER
			RL.GetCell(cell, false);
			if (cell == ".") cell.clear();
			GDS_Seq_AppendString(varFilter, cell.c_str());

			// column 8: INFO, skip
			RL.GetCell(cell, false);

			// column 9: FORMAT, skip
			RL.GetCell(cell, false);

			// #####################################################
			// columns for samples

			// for-loop
			for (int samp_idx=0; samp_idx < nTotalSamp; samp_idx++)
			{
				const char *pCh, *p;

				// read
				RL.GetCell(cell, samp_idx >= (nTotalSamp-1));

				// #################################################
				// the first field -- genotypes
				pCh = p = cell.c_str();
				while ((*p != 0) && (*p != ':')) p ++;
				value.assign(pCh, p);
				pCh = (*p == ':') ? (p + 1) : p;
				p = SKIP(value.c_str());

				// allele dosage
				int dosage = 0;

				while (*p)
				{
					char *endptr = (char*)p;
					int val = strtol(p, &endptr, 10);

					if (endptr == p)
					{
						if ((*p != '.') && RaiseError)
						{
							throw ErrCoreArray(
								"Invalid integer conversion \"%s\".",
								SHORT_TEXT(p).c_str());
						}
						val = -1;
					} else {
						if (val < 0)
						{
							val = -1;
							if (RaiseError)
							{
								throw ErrCoreArray(
									"Genotype code should be non-negative \"%s\".",
									SHORT_TEXT(p).c_str());
							}
						} else if (val >= num_allele)
						{
							val = -1;
							if (RaiseError)
							{
								throw ErrCoreArray(
									"Genotype code is out of range \"%s\".",
									SHORT_TEXT(p).c_str());
							}
						}
						p = endptr;
					}

					while ((*p != 0) && (*p != '|') && (*p != '/'))
						p ++;
					if ((*p == '|') || (*p == '/'))
						p ++;

					if (dosage >= 0)
					{
						if (val >= 0)
						{
							if (val == ref_allele_index)
								dosage ++;
						} else
							dosage = -1;
					}
				}

				if (dosage >= 0)
				{
					if (dosage >= 2) dosage = 2;
				} else
					dosage = 3;  // 3 -- missing value
				U8s[samp_idx] = dosage;
			}

			// write genotypes
			GDS_Seq_AppendData(varGeno, nTotalSamp, &U8s[0], svUInt8);
		}

		UNPROTECT(1);
		rv_ans = ScalarInteger(GDS_Variant_Index - old_variant_index);

	CORE_CATCH({
		char buf[4096];
		if (RL.ColumnNo() > 0)
		{
			snprintf(buf, sizeof(buf),
				"\nFILE: %s\n\tLINE: %d, COLUMN: %d, %s\n\t%s",
				fn, RL.LineNo(), RL.ColumnNo(), cell.c_str(),
				GDS_GetError());
		} else {
			snprintf(buf, sizeof(buf), "\nFILE: %s\n\tLINE: %d\n\t%s",
				fn, RL.LineNo(), GDS_GetError());
		}
		GDS_SetError(buf);
		has_error = true;
	});

	if (has_error) error(GDS_GetError());
	return rv_ans;
}



/** Oxford GEN --> SNP GDS
 *  \param vcf_fn            the file names of VCF format
 *  \param gds_root          the root of GDS file
 *  \param ReadLineFun       the calling
 *  \param ReadLine_Param    the parameter for calling function
 *  \param rho               the environment
 *  \param Verbose           print out information
 *  \return                  the number of variants
**/
COREARRAY_DLL_EXPORT SEXP gnr_Parse_GEN(SEXP gen_fn, SEXP gds_root,
	SEXP ChrCode, SEXP CallThreshold, SEXP ReadLineFun, SEXP ReadLine_Param,
	SEXP rho, SEXP Verbose)
{
	const char *fn = CHAR(STRING_ELT(gen_fn, 0));
	int verbose = asLogical(Verbose);
	if (verbose == NA_LOGICAL)
		error("'verbose' must be TRUE or FALSE.");
	const double CallProb = REAL(CallThreshold)[0];

	// define a variable for reading lines
	CReadLine RL;

	// string buffer
	string cell, value;
	cell.reserve(4096);
	value.reserve(4096);

	COREARRAY_TRY

		// *********************************************************
		// initialize variables		

		const bool RaiseError = true;

		// GDS nodes
		PdGDSObj Root = GDS_R_SEXP2Obj(gds_root);
		PdSequenceX varIdx = GDS_Node_Path(Root, "snp.id", TRUE);
		PdSequenceX varRSID = GDS_Node_Path(Root, "snp.rs.id", TRUE);
		PdSequenceX varPos = GDS_Node_Path(Root, "snp.position", TRUE);
		PdSequenceX varChr = GDS_Node_Path(Root, "snp.chromosome", TRUE);
		PdSequenceX varAllele = GDS_Node_Path(Root, "snp.allele", TRUE);
		PdSequenceX varGeno = GDS_Node_Path(Root, "genotype", TRUE);

		int nTotalSamp;
		{
			int D[2];
			GDS_Seq_GetDim(varGeno, D, 2);
			nTotalSamp = D[1];
		}

		// numeric buffer
		C_Int32 I32;

		// genotype buffer
		vector<C_UInt8> U8s;
		U8s.resize(nTotalSamp);

		int ChrValue1 = 0;
		string ChrValue2;
		if (IS_CHARACTER(ChrCode))
			ChrValue2 = CHAR(STRING_ELT(ChrCode, 0));
		else
			ChrValue1 = INTEGER(ChrCode)[0];

		// allele A and B
		string AlleleA, AlleleB;


		// *********************************************************
		// initialize external calling for reading stream

		SEXP R_Read_Call;
		PROTECT(R_Read_Call =
			LCONS(ReadLineFun, LCONS(ReadLine_Param, R_NilValue)));
		RL.Init(R_Read_Call, rho);
		RL.SplitBySpaceTab();

		// *********************************************************
		// parse the context

		int LN = 0;
		while (!RL.IfEnd())
		{
			// *****************************************************
			// scan line by line

			// column 1: SNP ID
			RL.GetCell(cell, false);
			GDS_Seq_AppendString(varIdx, cell.c_str());

			// column 2: RS ID
			RL.GetCell(cell, false);
			GDS_Seq_AppendString(varRSID, cell.c_str());

			// column 3: Base-pair position of the SNP
			RL.GetCell(cell, false);
			I32 = getInt32(cell, RaiseError);
			GDS_Seq_AppendData(varPos, 1, &I32, svInt32);

			// column 4, 5: A allele, B allele
			RL.GetCell(AlleleA, false);
			RL.GetCell(AlleleB, false);
			GDS_Seq_AppendString(varAllele, (AlleleA + "/" + AlleleB).c_str());

			if (IS_CHARACTER(ChrCode))
				GDS_Seq_AppendString(varChr, ChrValue2.c_str());
			else
				GDS_Seq_AppendData(varChr, 1, &ChrValue1, svInt32);

			// *****************************************************
			// columns for samples

			for (int samp_idx=0; samp_idx < nTotalSamp; samp_idx++)
			{
				// Read Prob(AA)
				RL.GetCell(cell, false);
				double P_AA = getFloat(cell, true);

				// Read Prob(AB)
				RL.GetCell(cell, false);
				double P_AB = getFloat(cell, true);

				// Read Prob(BB)
				RL.GetCell(cell, samp_idx >= (nTotalSamp-1));
				double P_BB = getFloat(cell, true);

				if (P_AA >= P_AB)
				{
					if (P_AA >= P_BB)
						U8s[samp_idx] = (P_AA >= CallProb) ? 2 : 3;
					else
						U8s[samp_idx] = (P_BB >= CallProb) ? 0 : 3;
				} else {
					if (P_AB >= P_BB)
						U8s[samp_idx] = (P_AA >= CallProb) ? 1 : 3;
					else
						U8s[samp_idx] = (P_BB >= CallProb) ? 0 : 3;
				}
			}

			// write genotypes
			GDS_Seq_AppendData(varGeno, nTotalSamp, &U8s[0], svUInt8);
			LN ++;
		}

		UNPROTECT(1);
		rv_ans = ScalarInteger(LN);

	CORE_CATCH({
		char buf[4096];
		if (RL.ColumnNo() > 0)
		{
			snprintf(buf, sizeof(buf),
				"\nFILE: %s\n\tLINE: %d, COLUMN: %d, %s\n\t%s",
				fn, RL.LineNo(), RL.ColumnNo(), cell.c_str(),
				GDS_GetError());
		} else {
			snprintf(buf, sizeof(buf), "\nFILE: %s\n\tLINE: %d\n\t%s",
				fn, RL.LineNo(), GDS_GetError());
		}
		GDS_SetError(buf);
		has_error = true;
	});

	if (has_error) error(GDS_GetError());
	return rv_ans;
}

} // extern "C"