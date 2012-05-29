/*
 * This file contains functions that allow the user to execute one calculation
 * of UIS or minimal number of transitions needed for a peptide with one call.
 * This means that instead of calling several functions after each other and
 * passing data structures between Python and Cpp, all can be done in Cpp which
 * is faster.
 *
 * Thus it combines the functionality of rangetree.cpp and getNonUis.cpp in one.
 *
*/

/*
 *
 * Program       : SRMCollider
 * Author        : Hannes Roest <roest@imsb.biol.ethz.ch>
 * Date          : 05.02.2011 
 *
 *
 * Copyright (C) 2011 Hannes Roest
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA
 *
 */


//include our own libraries
#include "rangetree.cpp"
#include "srmcollider.h"
#include "combinatorics.h"
#include "srmcolliderLib.cpp"

// Including headers from CGAL 
#include <CGAL/Cartesian.h>
#include <CGAL/Range_segment_tree_traits.h>
#include <CGAL/Range_tree_k.h>

#include <vector>

// Function declarations
//minimally needed number of transitions to get a UIS (ordered transitions)
int min_needed(python::tuple transitions, python::tuple precursors,
    int max_uis, double q3window, bool ppm, python::object par);

//return the number of non UIS present given the transitions and a query window
//in Q1 and SSRCalc
// using magic gives a speedup of around 2fold
python::list wrap_all_magic(python::tuple transitions, double a, double b,
        double c, double d, long thispeptide_key, int max_uis, double q3window,
        bool ppm, int max_nr_isotopes, double isotope_correction, python::object par,
        SRMCollider::ExtendedRangetree::Rangetree_Q1_RT& rtree
        );
python::list wrap_all(python::tuple transitions, double a, double b, double c,
        double d, long thispeptide_key, int max_uis, double q3window, bool ppm);

struct Transition{
  double q3;
  long srm_id;
};

/* 
 * Calculate the minimally needed number of transitions needed to get a UIS
 * given transitions in their preferred order and interfering precursors.
 *
 * Uses integers with bitflags to store the combinations, thus the number of
 * transitions that can be considered is limited by COMBLIMIT.  Also note that
 * the leftmost bit needs to be set to zero all the time, otherwise an infinte
 * loop occurs, thus there is one bit we cannot use.
*/
int min_needed(python::tuple transitions, python::tuple precursors,
    int max_uis, double q3window, bool ppm, python::object par)   {

    //use the defined COMBINT (default 32bit int) and some magic to do this :-)
    COMBINT one;
    COMBINT currenttmp = 0;
    std::vector<COMBINT> newcollperpep;

    python::tuple clist;
    int fragcount, i, k, ch, j;
    long srm_id;
    double q3, q3used = q3window;
    char* sequence;

    Transition transition;

    double* series = new double[1024];
    double* tmp_series = new double[1024];

    bool aions      =  python::extract<bool>(par.attr("aions"));
    bool aMinusNH3  =  python::extract<bool>(par.attr("aMinusNH3"));
    bool bions      =  python::extract<bool>(par.attr("bions"));
    bool bMinusH2O  =  python::extract<bool>(par.attr("bMinusH2O"));
    bool bMinusNH3  =  python::extract<bool>(par.attr("bMinusNH3"));
    bool bPlusH2O   =  python::extract<bool>(par.attr("bPlusH2O"));
    bool cions      =  python::extract<bool>(par.attr("cions"));
    bool xions      =  python::extract<bool>(par.attr("xions"));
    bool yions      =  python::extract<bool>(par.attr("yions"));
    bool yMinusH2O  =  python::extract<bool>(par.attr("yMinusH2O"));
    bool yMinusNH3  =  python::extract<bool>(par.attr("yMinusNH3"));
    bool zions      =  python::extract<bool>(par.attr("zions"));
    bool MMinusH2O  =  python::extract<bool>(par.attr("MMinusH2O"));
    bool MMinusNH3  =  python::extract<bool>(par.attr("MMinusNH3"));

    int precursor_length = python::extract<int>(precursors.attr("__len__")());
    int transitions_length = python::extract<int>(transitions.attr("__len__")());
    
    //Check whether we have more transitions than we have bits in our number
    if (transitions_length > COMBLIMIT) {
        PyErr_SetString(PyExc_ValueError, 
            "Too many transitions, please adjust limit.");
        boost::python::throw_error_already_set();
        return -1;
    }

    /*
    * Transitions are tuples of the form (q3, srm_id)
    * convert to our struct.
    */
    python::tuple tlist;
    vector<Transition> mytransitions(transitions_length);
    for (i=0; i<transitions_length; i++) {
        tlist = python::extract< python::tuple >(transitions[i]);
        q3 = python::extract<double>(tlist[0]);
        srm_id = python::extract<long>(tlist[1]);
        struct Transition entry = {q3, srm_id};
        mytransitions[i] = entry;
    }

    // Go through all (potential) collisions 
    //
    int maxoverlap = 0;
    for (j=0; j<precursor_length; j++) {
      // TODO do N15
        clist = python::extract< python::tuple >(precursors[j]);
        sequence = python::extract<char *>(clist[1]);
        for (ch=1; ch<=2; ch++) {
            fragcount = _calculate_clashes_other_series_sub(sequence, tmp_series, series, ch,
                aions, aMinusNH3, bions, bMinusH2O,
                bMinusNH3, bPlusH2O, cions, xions, yions, yMinusH2O,
                yMinusNH3, zions, MMinusH2O, MMinusNH3);
            for(int i = 0; i != transitions_length; i++) {
                //ppm is 10^-6
                transition = mytransitions[i];
                q3 = transition.q3;
                if(ppm) {q3used = q3window / 1000000.0 * q3; } 
                    // go through all fragments of this precursor
                    for (k=0; k<fragcount; k++) {
                        if(fabs(q3-series[k]) < q3used ) {
                            //left bitshift == 2^i
                            one = 1;
                            currenttmp |= one << i;
                        }
                    }
                } //loop over all transitions
            } //end loop over all charge states of this precursor
        if ( currenttmp ) {
            //while we find the transitions from our relative order also in the
            //peptide we just looked at, increase i
            one = 1;
            i = 0;
            while( currenttmp & one << i ) i++;
            if( i > maxoverlap ) maxoverlap = i;
            currenttmp = 0;
        }
    }

    //we have counted from 0 to N-1, but now want the number of transitions
    ++maxoverlap;
    if(maxoverlap > transitions_length) {return -1;}
    return maxoverlap;
}

/*
 * Return the number of non-UIS for all orders up to max_uis
 * Given the transitions and then four numbers giving the coordinate window in
 * which the precursors should be found: 
 *   q1_low, ssrcalc_low, q1_high, ssrcalc_high, 
 * Also the peptide key of the current peptide has to be given (to exclude it)
 * as well as the maximal order of UIS to compute, the used q3 window and
 * whether the q3 window is given in ppm (parts per million), default unit is
 * Th which is m/z
 *
 * This function uses "magic" to speed up the calculation: each combination of
 * transitions is encoded in an integer, which allows easy storage and
 * computation by bitshits even though the code gets less easy to understand.
 * The default length of the integer is 32 bits thus we can at most handle
 * combinations for 31 transitions; if more are needed one needs to change
 * COMBINT to an integer format that usees more bits, e.g. unit64_t or even the
 * BigInt class for larger integers.
 * If there are more transitions provided than allowed, an error will be
 * thrown. 
*/
python::list wrap_all_magic(python::tuple transitions, double a, double b,
  double c, double d, long thispeptide_key, int max_uis, double q3window,
  bool ppm, int max_nr_isotopes, double isotope_correction, python::object par,
  SRMCollider::ExtendedRangetree::Rangetree_Q1_RT& rtree)
{
    //use the defined COMBINT (default 32bit int) and some magic to do this :-)
    COMBINT one;
    COMBINT currenttmp = 0;
    std::vector<COMBINT> newcollperpep;

    int fragcount, i, k, ch;
    long srm_id;
    double q3, q3used = q3window;
    char* sequence;

    Transition transition;
    SRMCollider::ExtendedRangetree::Precursor precursor;
    std::vector<SRMCollider::ExtendedRangetree::Key> OutputList;

    double* b_series = new double[256];
    double* y_series = new double[256];

    int iso;
    double q1_low = a; double q1_high = c;

    bool aions      =  python::extract<bool>(par.attr("aions"));
    bool aMinusNH3  =  python::extract<bool>(par.attr("aMinusNH3"));
    bool bions      =  python::extract<bool>(par.attr("bions"));
    bool bMinusH2O  =  python::extract<bool>(par.attr("bMinusH2O"));
    bool bMinusNH3  =  python::extract<bool>(par.attr("bMinusNH3"));
    bool bPlusH2O   =  python::extract<bool>(par.attr("bPlusH2O"));
    bool cions      =  python::extract<bool>(par.attr("cions"));
    bool xions      =  python::extract<bool>(par.attr("xions"));
    bool yions      =  python::extract<bool>(par.attr("yions"));
    bool yMinusH2O  =  python::extract<bool>(par.attr("yMinusH2O"));
    bool yMinusNH3  =  python::extract<bool>(par.attr("yMinusNH3"));
    bool zions      =  python::extract<bool>(par.attr("zions"));
    bool MMinusH2O  =  python::extract<bool>(par.attr("MMinusH2O"));
    bool MMinusNH3  =  python::extract<bool>(par.attr("MMinusNH3"));

    //Check whether we have more transitions than we have bits in our number
    int transitions_length = python::extract<int>(transitions.attr("__len__")());
    if (transitions_length > COMBLIMIT) {
        PyErr_SetString(PyExc_ValueError, 
            "Too many transitions, please adjust limit.");
        boost::python::throw_error_already_set();
        python::list tlist;
        return tlist;
    }

    /*
    * Transitions are tuples of the form (q3, srm_id)
    * convert to our struct.
    */
    python::tuple tlist;
    vector<Transition> mytransitions(transitions_length);
    for (i=0; i<transitions_length; i++) {
        tlist = python::extract< python::tuple >(transitions[i]);
        q3 = python::extract<double>(tlist[0]);
        srm_id = python::extract<long>(tlist[1]);
        struct Transition entry = {q3, srm_id};
        mytransitions[i] = entry;
    }

    // search for all matching peptides (including all potential isotopes)
    SRMCollider::ExtendedRangetree::Interval win(SRMCollider::ExtendedRangetree::Interval(
      SRMCollider::ExtendedRangetree::K::Point_2(a-isotope_correction,b),
      SRMCollider::ExtendedRangetree::K::Point_2(c,d)));
    rtree.my_rangetree->window_query(win, std::back_inserter(OutputList));
    std::vector<SRMCollider::ExtendedRangetree::Key>::iterator current=OutputList.begin();

    double* series = new double[1024];
    double* tmp_series = new double[1024];

    // Go through all (potential) collisions we just extracted from the rangetree
    //
    // This assumes that we do not have peptide_keys mapped to different
    // colliding transitions. In fact we do not have this situation even though
    // we have duplicate peptide_keys (from the isotopes). But they will
    // produce the same interefering transitions and thus the same entry in the
    // collisions per peptide table.
    while(current!=OutputList.end()){

            double q1 = current->first[0];
            int charge = current->second.q1_charge;
            bool proceed = false;
            // check whether there are any relevant isotopes, otherwise exclude this hit
            for (iso=0; iso<=max_nr_isotopes; iso++) {
                if (q1 + (MASS_DIFFC13 * iso)/charge > q1_low && 
                      q1 + (MASS_DIFFC13 * iso)/charge < q1_high) 
                {
                    proceed = true;
                }
            }

            if (!proceed || thispeptide_key == current->second.peptide_key) {
              // go to next
              current++;
              continue;
            }

            precursor =  current->second;
            sequence = precursor.sequence;
            for (ch=1; ch<=2; ch++) {

                fragcount = _calculate_clashes_other_series_sub(sequence, tmp_series, series, ch,
                  aions, aMinusNH3, bions, bMinusH2O,
                  bMinusNH3, bPlusH2O, cions, xions, yions, yMinusH2O,
                  yMinusNH3, zions, MMinusH2O, MMinusNH3, current->second.isotope_modification);

                for (i=0; i<transitions_length; i++) {
                    transition = mytransitions[i];
                    q3 = transition.q3;
                    //ppm is 10^-6
                    if(ppm) {q3used = q3window / 1000000.0 * q3; } 

                        // go through all fragments of this precursor
                        for (k=0; k<fragcount; k++) {

                            if(fabs(q3-series[k]) < q3used ) {
                            
                                //left bitshift == 2^i
                                one = 1;
                                currenttmp |= one << i;
                            }
                        }
                } //loop over all transitions
            }


            //Store current combination
            if ( currenttmp ) {
                newcollperpep.push_back(currenttmp);
                currenttmp = 0;
            }


      current++;
    }

    //this takes about 50% or more of the time if we have many collisions_per_pep (10k)
    //and below 10% if we have few collision_per_pep (0.1k)
    std::vector<int> c_result;
    for(i =1; i<= max_uis; i++) {
      std::set<COMBINT> combinations;
      get_non_uis_magic(newcollperpep, transitions_length, i, combinations);
      c_result.push_back(combinations.size());
    }

    delete [] b_series;
    delete [] y_series;

    python::list result;
    for (size_t i = 0; i < c_result.size(); i++)
    {
      result.append(c_result[i]);
    }
    return result;
}

// Expose to Python
using namespace python;
BOOST_PYTHON_MODULE(c_integrated)
{

    def("wrap_all_magic", wrap_all_magic,
            
 "Return the number of non-UIS for all orders up to max_uis\n"
 "Given the transitions and then four numbers giving the coordinate window in\n"
 "which the precursors should be found: \n"
 "  q1_low, ssrcalc_low, q1_high, ssrcalc_high, \n"
 "Also the peptide key of the current peptide has to be given (to exclude it)\n"
 "as well as the maximal order of UIS to compute, the used q3 window and\n"
 "whether the q3 window is given in ppm (parts per million), default unit is\n"
 "Th which is m/z\n"
 "\n"
 "This function uses magic to speed up the calculation: each combination of\n"
 "transitions is encoded in an integer, which allows easy storage and\n"
 "computation by bitshits even though the code gets less easy to understand.\n"
 "The default length of the integer is 32 bits thus we can at most handle\n"
 "combinations for 31 transitions; if more are needed one needs to change\n"
 "COMBINT to an integer format that usees more bits, e.g. unit64_t or even the\n"
 "BigInt class for larger integers.\n"
 "If there are more transitions provided than allowed, an error will be\n"
 "thrown. \n"
 "\n"
 "\n"
 " Signature\n"
 "list wrap_all_magic(python::tuple transitions, double a, double b,\n"
        "double c, double d, long thispeptide_key, int max_uis, double q3window,\n"
        "bool ppm, int max_nr_isotopes, double isotope_correction)"
    );

    def("getMinNeededTransitions", min_needed,
            
 "Calculate the minimally needed number of transitions needed to get a UIS\n"
 "given transitions in their preferred order and interfering precursors.\n"
 "\n"
 "Uses integers with bitflags to store the combinations, thus the number of\n"
 "transitions that can be considered is limited by COMBLIMIT.  Also note that\n"
 "the leftmost bit needs to be set to zero all the time, otherwise an infinte\n"
 "loop occurs, thus there is one bit we cannot use.\n"
 "\n"
 "\n"
 " Signature\n"
 "int min_needed(tuple transitions, tuple precursors,\n"
    "int max_uis, double q3window, bool ppm )\n"
            );
}
