#ifndef CLASSIFICATOR_C
#define CLASSIFICATOR_C
#include "Classificator.hpp"

#define P_CHECK_OVERFLOW if(p.overflow_flag()) cout<<BKG_YELLOW<<BLACK<<"WARNING"<<BKG_RST<<D_YELLOW<<" OVERFLOW DETECTED IN CORE"<<RST<<endl;
#define R_CHECK_OVERFLOW if(res_v[core].overflow_flag()) cout<<BKG_YELLOW<<BLACK<<"WARNING"<<BKG_RST<<D_YELLOW<<" OVERFLOW DETECTED IN CORE"<<RST<<endl;
#define A_CHECK_OVERFLOW if(acc.overflow_flag()) cout<<BKG_YELLOW<<BLACK<<"WARNING"<<BKG_RST<<D_YELLOW<<" OVERFLOW DETECTED IN CORE"<<RST<<endl;


#define SV0 0
#define SV1 (466*(SV_LEN+1))+1
#define SV2 SV1+(408*(SV_LEN+1))+1
#define SV3 SV2+(827*(SV_LEN+1))+1
#define SV4 SV3+(948*(SV_LEN+1))+1
#define SV5 SV4+(683*(SV_LEN+1))+1
#define SV6 SV5+(756*(SV_LEN+1))+1
#define SV7 SV6+(533*(SV_LEN+1))+1
#define SV8 SV7+(659*(SV_LEN+1))+1
#define SV9 SV8+(1078*(SV_LEN+1))+1

const array<int, 10> sv_start_addr = {SV0, SV1, SV2, SV3, SV4, SV5, SV6, SV7, SV8, SV9};

Classificator::Classificator(sc_module_name name): sc_module(name),
                                                   tsoc("classificator_tsoc"),
                                                   isoc("classificator_isoc"),
                                                   dmi_valid(false)
{
   tsoc(*this);
   isoc(*this);
   cout<<name<<" constucted"<<endl;
   image_v.reserve(SV_LEN);
   res_v.reserve(10);
}



void Classificator::b_transport(pl_t& pl, sc_time& offset)
{
	
   tlm_command cmd    = pl.get_command();
   uint64 adr         = pl.get_address();
   unsigned char *buf = pl.get_data_ptr();
   unsigned int len   = pl.get_data_length();

   if(adr!=1)
   {
      SC_REPORT_ERROR("Classificator"," Not my payload ");
      return;
   }

   switch(cmd)
   {

   case TLM_WRITE_COMMAND://**********CLASSIFY IMAGE**********
         
      image_v.clear();
      for(unsigned int i=0; i<len; i++)
         image_v.push_back(((din_t *)buf)[i]);

      cmd=TLM_READ_COMMAND;
      pl.set_command         ( cmd );
      pl.set_data_ptr        ( buf );
      res_v.clear();
      for(unsigned int core=0; core<10; core++)
      {
         acc=0;

         for( int sv=0; sv<sv_array[core]; sv++)
         {
            //REQUEST SV
            adr=sv_start_addr[core]+sv*SV_LEN;
            pl.set_address ((uint64)adr);
            len=SV_LEN;
            pl.set_data_length (len);
            pl.set_response_status ( TLM_INCOMPLETE_RESPONSE );
            isoc->b_transport(pl,offset);

            if (pl.get_response_status() != TLM_OK_RESPONSE)
               SC_REPORT_INFO("Classificator","WARNING: WRONG RESPONSE");
               
            buf=pl.get_data_ptr();

            p=1.0;
            for( int i=0; i<SV_LEN; i++)
            {
               p+=image_v[i]*((din_t*)buf)[i];
               P_CHECK_OVERFLOW
            }
            p*=0.1;
            P_CHECK_OVERFLOW

            p=p*p*p;
            P_CHECK_OVERFLOW
               
            //REQUEST LAMBDA
            adr=sv_start_addr[core]+sv_array[core]*SV_LEN+sv;
            pl.set_address (adr);
            len=1;
            pl.set_data_length (len);
            pl.set_response_status ( TLM_INCOMPLETE_RESPONSE );
            isoc->b_transport(pl,offset);

            if (pl.get_response_status() != TLM_OK_RESPONSE)
               SC_REPORT_INFO("Classificator","WARNING: WRONG RESPONSE");
               
            buf=pl.get_data_ptr();

            p*= *(lin_t*)buf;
            P_CHECK_OVERFLOW
               
               acc+=p;
            A_CHECK_OVERFLOW
         }

         //REQUEST BIAS
         adr=sv_start_addr[core]+sv_array[core]*(SV_LEN+1);
         pl.set_address (adr);
         len=1;
         pl.set_data_length (len);
         pl.set_response_status ( TLM_INCOMPLETE_RESPONSE );
         isoc->b_transport(pl,offset);

         if (pl.get_response_status() != TLM_OK_RESPONSE)
            SC_REPORT_INFO("Classificator","WARNING: WRONG RESPONSE");

         buf=pl.get_data_ptr();
         acc+=*(bin_t*)buf;
         A_CHECK_OVERFLOW

         res_v.push_back (acc);
         R_CHECK_OVERFLOW
         
         }

      max_res=res_v[0];
      cl_num=0;
      for(int i=1; i<10; i++)
      {
         if(max_res<res_v[i])
         {
            max_res=res_v[i];
            cl_num=(num_t)i;
         }
      }

      offset+=sc_time(100, SC_NS);
      pl.set_response_status( TLM_OK_RESPONSE );
      break;

   case TLM_READ_COMMAND://--------------------------------------------------------RETURN RESULTS
         
      buf=(unsigned char *)&cl_num;
      pl.set_data_ptr        ( buf );
      pl.set_response_status( TLM_OK_RESPONSE );
      offset+=sc_time(5, SC_NS);
      break;

   default:
      pl.set_response_status( TLM_COMMAND_ERROR_RESPONSE );
   }

}


tlm_sync_enum Classificator::nb_transport_fw(pl_t& pl, phase_t& phase, sc_time& offset)
{
   return TLM_ACCEPTED;
}

bool Classificator::get_direct_mem_ptr(pl_t& pl, tlm_dmi& dmi)
{
   dmi.allow_read_write();
   return true;
}

unsigned int Classificator::transport_dbg(pl_t& pl)
{
   return 0;
}

tlm_sync_enum Classificator::nb_transport_bw(pl_t& pl, phase_t& phase, sc_time& offset)
{
   return TLM_ACCEPTED;
}

void Classificator::invalidate_direct_mem_ptr(uint64 start, uint64 end)
{
   dmi_valid = false;
}
#endif
