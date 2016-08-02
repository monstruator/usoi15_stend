#include <stdio.h>
#include <errno.h>
#include <sys/time.h>

#include "globals.h"
extern int verbose;

int inbuf2step = STEP_MARK;


int HandlerInBuf2( void )
{
   static int ip;
   static struct packet12 *pack;
   unsigned short n;

   if( verbose > 1 ) {
      printf( "HandlerInBuf2: save-load=%d.\n", inbuf2.save - inbuf2.load );
   }

   while( inbuf2.load != inbuf2.save ) {
      if( inbuf2step == STEP_NONE ) {
         inbuf2.save = inbuf2.load;
      }
//	printf("inbuf2step=%d\n",inbuf2step);

      if( inbuf2step == STEP_MARK ) {
         if( MarkInBuf2() ) {
            ip = inbuf2.load;
            pack = (struct packet12 *)&inbuf2.data[ip];
            inbuf2step = STEP_HEADER;
         }
      }
	//printf("inbuf2step=%d\n",inbuf2step);

      if( inbuf2step == STEP_HEADER ) {
         if( HeadInBuf2() ) {
			//printf("kss=%d rez13=%d \n",pack->head.kss,pack->head.rez13);
            if( pack->head.kss > 0 ) {
               inbuf2step = STEP_DATA;
               n = pack->head.kss;
               DataInBuf2( &n );
            } else {
               inbuf2step = STEP_HANDLER;
            }
         }
      }
	//printf("inbuf2step=%d\n",inbuf2step);

      if( inbuf2step == STEP_DATA ) {
		switch( DataInBuf2( NULL ) ) {
            case 1 : inbuf2step = STEP_HANDLER; break;
			case 2 : inbuf2step = STEP_MARK;
					 return 0;
         }
      }
	//printf("inbuf2step=%d\n",inbuf2step);

      if( inbuf2step == STEP_HANDLER ) {
         outpack1.blk &= ~BUF3KIT_BLK2;
         outpack2.blk &= ~BUF3KIT_BLK2;
         outpack3.blk &= ~BUF3KIT_BLK2;
         outpack4.blk &= ~BUF3KIT_BLK2;
         outpack5.blk &= ~BUF3KIT_BLK2;
         outpack6.blk &= ~BUF3KIT_BLK2;
 
         HandlerInPack2( pack , inbuf2.load - ip );
         if( outpack2.nload >= outpack2.nsave ) {
            outpack2.nsave = outpack2.nload = 0;
            stat.in |= FLAG_BUF2;
            CheckStatus();
            ControlLed2( 0 );
         }
         SendOutPack2();
		//	printf("SendOutPack\n");
         inbuf2step = STEP_MARK;
      }
	//printf("inbuf2step=%d\n",inbuf2step);

   }

   memset( (char *)inbuf2.data, 0, sizeof(inbuf2.data) );
   inbuf2.save = inbuf2.load = 0;

   return( inbuf2step );
}

int MarkInBuf2( void )
{
   unsigned char b;
   int i;
   static int nmark = 0;

   while( inbuf2.load < inbuf2.save ) {

      i = inbuf2.load;
      b = inbuf2.data[i];

      switch( nmark ) {
      case 0:
         if( b == 0xff ) {
            nmark = 1;
         }
         break;
      case 1:
         if( b == 0xff ) {
            nmark = 2;
         } else {
            nmark = 0;
         }
         break;
      case 2:
         if( b == 0xff ) {
            nmark = 3;
         } else {
            nmark = 0;
         }
         break;
      case 3:
         if( b == 0xff ) {
            nmark = 4;
         } else {
            nmark = 0;
         }
         break;
      default:
         nmark = 0;
         break;
      }
         
      inbuf2.load++;

      if( nmark >= 4 ) {
         inbuf2.load -= nmark;
         nmark = 0;
         return( 1 );
      }
   }

   return( 0 );
}

int HeadInBuf2( void )
{
   static unsigned int nhead = sizeof(struct header12);

   if( inbuf2.save - inbuf2.load >= nhead) {
      inbuf2.load += nhead;
      nhead = sizeof(struct header12);
      return( 1 );
   } else {
      inbuf2.load = inbuf2.save;
      nhead -= inbuf2.save - inbuf2.load; 
      return( 0 );
   }
}

int DataInBuf2( unsigned short *nd )
{
   static unsigned int ndata = 0;

   if( nd != NULL ) {
      ndata = *nd * 2;
      return( 0 );
   }

   if( inbuf2.save - inbuf2.load >= ndata) {
      inbuf2.load += ndata;
      ndata = 0;
      return( 1 );
   } else {
  	  //printf("ndata=%d s=%d l=%d\n",ndata,inbuf2.save,inbuf2.load);
      //ndata -= inbuf2.save - inbuf2.load; 
      inbuf2.load -= 32; //���㫨 ����� ������
		memcpy(&inbuf2.data[0],&inbuf2.data[inbuf2.load],inbuf2.save-inbuf2.load);
		inbuf2.save-=inbuf2.load;
		inbuf2.load=0;
		//printf("ndata=%d s=%d l=%d\n",ndata,inbuf1.save,inbuf1.load);
      return( 2 ); //�� ��������� �ਭ�� �����
   }
}


int HandlerInPack2( struct packet12 *pack, int size )
{
   struct statkasrt *sk;
   struct sostrts *sr;
   struct errusoi *ko;
   struct form12 *f12;
   struct sac *fs;
   struct sac *f27;
   struct form199 *f199;
   struct form18 *f18;
   struct form18a *f18a;
   struct form18b *f18b;
   struct form_reo *freo;

	static short paket_rli;
   unsigned fsa;
   unsigned fsp;
   unsigned fsr;
   unsigned fsv;
   unsigned fsn;
   short *form_num;
   int i;
   char b[sizeof(struct form199)];

   if( verbose > 1 ) {
      printf( "HandlerInPack2: size=%d kzo=%d kss=%d kvi=%d ps=%d\n",
         size, pack->head.kzo, pack->head.kss, pack->head.kvi, pack->head.ps );
      printf( "HandlerInPack2: wf= %04x %04x %04x %04x %04x %04x %04x\n",
         pack->wf[0], pack->wf[1], pack->wf[2], pack->wf[3], pack->wf[4],
         pack->wf[5], pack->wf[6] ); 
   }

   sk = (struct statkasrt *)&outpack0.word_sost_kasrt2_1;
   sr = (struct sostrts *)&outpack0.word_sost_rts_1;
   ko = (struct errusoi *)&outpack0.k_o;

   //********* (24.06.2011) *********

 //  if( pack->head.ps == 0 ) {
 //     outpack1.blk |= BUF3KIT_BLK1;
 //  }

   //*********************************

   if( size > sizeof(struct header12) ) {
      switch( pack->head.kvi ) {
      case 3:
         f12 = (struct form12 *)pack->wf;
         sk->s0got = f12->s0got;
         sk->s0prm = f12->s0prm;
         sk->s0prd = f12->s0prd;
         sk->s0cpp = f12->s0cpp;
         sk->s1fk = f12->s1tr;
         sk->s1fm = f12->s1m > 1 ? 1 : 0;
         sk->s1prm = f12->s1rp;
         sk->s1rab = f12->s1vr;
//         sk->s1rab = ~f12->s1vr; //???????????????????
         sk->s2prd = ( f12->s2prd ) & 0x7;
         sk->s2prm = ( f12->s2prm ) & 0xf;
         sk->rez1 = 0;
         sk->s3prd = ( f12->s3 ) & 0xf;
         sk->s4fk = ( f12->s4 ) & 0xf;
         sk->s5ss = f12->s5ss;
         sk->s5mi = f12->s5mi;
         sk->s5ib = f12->s5ib;
         sk->s5erib = f12->s5erib;
         sk->s6upr = f12->s6upr;
         sk->s6inf = f12->s6inf; //s6inf=1 - error write data for SVC 
         sk->s6prd = f12->kprd;
         sk->s7inf = f12->s7;
         sk->s8erib = ( f12->s8 ) & 0xff;
         sk->s9pream = ( f12->s9pr ) & 0xffff;
         sk->rez2 = 0;
//         sk->s9ss = ( f12->s9ss ) & 0xffff;
//         sk->s9mi = ( f12->s9mi ) & 0xffff;
         sr->svc2_rej = f12->s1tr;
         sr->svc2_m = f12->s1m > 1 ? 1 : 0;
         sr->svc2_rejprm = f12->s1rp;
         sr->svc2_rab = f12->s1vr;
         sr->svc2_prd = ( f12->s2prd ) & 0xf;
         sr->svc2_prm = ( f12->s2prm ) & 0xf;
         sr->svc2_oslprd = ( f12->s3 ) & 0xf;
         sr->svc2_fk = ( f12->s4 ) & 0xf;
         if( sk->s0cpp == 1 ) {
            outpack0.kzv = 1;
            ko->cpp1 = 1;
         }
		if (verbose >1 )	
		{
			printf("prd=%d prm=%d 9pr=%d s1tr=%d s1m=%d s1rp=%d s1vr=%d \n",
	        	 sk->s2prd,sk->s2prm,sk->s9pream,sk->s1fk,sk->s1fm,sk->s1prm,sk->s1rab);
			printf("s6upr=%d sk->s6inf=%d sk->s6prd=%d \n",
         		sk->s6upr, 
         		sk->s6inf,  
         		sk->s6prd);
		}
			if ((mode.rli2) || (mode.scan2))
			{
				kzo7_2();	
				//stat.out|=FLAG_BUF1; //�� ���� �.�. ��� �������
			}
         break;
      default:
         outpack0.link = KRK_DATA_OK; //�ᥣ�� ����=6 �� ��室� ������ (��⮬ �஢���� �� ���)
         fs = (struct sac *)pack->wf;
         fsa = fs->a0 + fs->a1 * 10 + fs->a2 * 100 + fs->a3 * 1000 + 
            fs->a4 * 10000 + fs->a5 * 100000;
         fsp = fs->p0 + fs->p1 * 10 + fs->p2 * 100 + fs->p3 * 1000 + 
            fs->p4 * 10000 + fs->p5 * 100000;
         fsr = fs->r0 + fs->r1 * 10 + fs->r2 * 100 + fs->r3 * 1000;
         fsv = fs->v0 + fs->v1 * 10 + fs->v2 * 100 + fs->v3 * 1000;
         fsn = *(short *)( (char *)fs + sizeof(struct sac) );
         if( verbose > 0 ) {
            printf( "SVCH2: SAC f=%d k=%d a=%d p=%d r=%d v=%d n=%d.\n", 
               fs->nf, fs->kvi, fsa, fsp, fsr, fsv, fsn );
            printf( "SVCH2: MODE scan=%d rli=%d addr=%d.\n",
               mode.scan2, mode.rli2, mode.addr2 );
         }
//         if( ( fsa != mode.addr2 ) || !mode.scan2 ) {
/*         if( fsa != mode.addr2 ) {
            if( verbose > 0 ) {
               printf( "SVCH2: Ignore packet.\n" );
            }
            break;
         }
*/
        if( fs->nf == 18 ) 
		{
            memcpy( &outpack0.svch2.sach18, fs, sizeof(struct sac) );
       	    outpack0.svch2.cr++;
            if( fsn > 4082 ) fsn = 4082;
			if ((mode.rli2)&&(fs->kvi==9)) //����祭 ०�� ���ᮢ ��� � ��襫 ���
			{
				if (outpack0.svch2_rli.nword==0)  //��ࢠ� ��ப�
				{
					outpack0.svch2.nword = fsn+1;//ࠧ��� ����� + num ()
					//outpack0.svch2_rli.nword = fsn; //���� ��� �㬠
				}
				else 
				{
					//᪮��஢��� ��⮬ �� ��᫥���� form5
					outpack0.svch2_rli.nword += fsn-6; //������塞 ⮫쪮 ��ப� (�������) //�뫮 21
					//outpack0.svch2.nword += fsn; //������塞 ⮫쪮 ��ப� (ॠ�쭮� ���)
				}
				printf("�� %d ����� ૨2. �ᥣ� %d fsn=%d\n",outpack0.svch2_rli.num,paket_rli,fsn);

	            memcpy( &outpack0.svch2_rli.form6[outpack0.svch2_rli.num*203],(char *)fs+sizeof(struct sac) + 14 , 406); //form6 //44
	            memcpy( &outpack0.svch2_rli.form5, (char *)fs+sizeof(struct sac) + 2 , 12); //form5

				for(i=0;i<10;i++) outpack0.svch2_rli.form1[i]=mode.cf1_svch2[i];
				for(i=0;i<5;i++)  outpack0.svch2_rli.form2[i]=mode.cf2_svch2[i];
				
//	            memcpy( &outpack0.svch2_rli.form6[outpack0.svch2_rli.num*203],(char *)fs, 406); //form6 //24
//!	            for (i=0;i<203;i++) printf(" %04x ",outpack0.svch2_rli.form6[outpack0.svch2_rli.num*203+i]);printf("\n");
//!	            printf(" %04x ",outpack0.svch2_rli.form6[outpack0.svch2_rli.num*203+1]>>7);printf("\n");

//	            for (i=0;i<203;i++) printf(" %04x ",*pack->wf+i);printf("\n");

				printf("nword=%d\n",outpack0.svch2_rli.nword);					
				printf("%d ����� ૨2. �ᥣ� %d fsn=%d\n",outpack0.svch2_rli.num,paket_rli,fsn);
				outpack0.svch2_rli.num++;
				paket_rli++;
			}
			else if (mode.scan2)
				switch (fs->kvi)
				{
					case 5 : 
							//printf(" outpack0.link = %d", outpack0.link);

				       if( stat.link ) {
				    		//outpack0.link = KRK_CMD_OK;
							//stat.link=0;
           				}

						printf("nword=%d tki kvi=5 recieve\n",outpack0.svch2_no.nword);					
			            //for (i=6;i<32;i++) printf(" %04x ",*(pack->wf+i));printf("\n");

			            f18 = (struct form18 *)fs;
						printf(" fsn= %x \n ",f18->fsn);
						//printf(" form_num= %x \n ",f18->cf1[0]>>11);
						//printf(" form_num= %x \n ",f18->cf2[0]>>11);
						//printf(" form_num= %x \n ",f18->cf3[0]);

						if (f18->cf1[0]>>11 == 1) //est' Form1
						{
							printf("est' form1\n");
							for(i=0;i<10;i++) outpack0.svch2_no.form1[i]=f18->cf1[i];
							if (outpack0.svch2_no.nword==0) 
								outpack0.svch2_no.nword += 10; 

							if (f18->cf2[0]>>11 == 2) // est' Form2
							{
								printf("est' form12\n");
								for(i=0;i<5;i++) outpack0.svch2_no.form2[i]=f18->cf2[i];
								if (outpack0.svch2_no.nword==10) 
									outpack0.svch2_no.nword += 5; 

								if (f18->cf3[0]>>11 == 3) // est' Form3 (kopiruem vse Form3)
								{
									printf("est' form123 %d \n",outpack0.svch2.nword); 
									for(i=0;i<f18->fsn - 15;i++) 
										outpack0.svch2.word[outpack0.svch2.nword+i]=f18->cf3[i];
									outpack0.svch2_no.nword += f18->fsn-15; //
								}	
							}
							else
							if (f18->cf2[0]>>11 == 3) // est' Form3 bez Form2
							{
								printf("est' form3 bez form2 %d \n",outpack0.svch2.nword);
								if (outpack0.svch2_no.form2[0]>>11 == 2) // bil Form2
								{
									f18a = (struct form18a *)fs;

									for(i=0;i<f18->fsn - 10;i++) 
										outpack0.svch2.word[outpack0.svch2.nword+i]=f18a->cf3[i];
									outpack0.svch2_no.nword += f18->fsn-10; //							
								}
							}
						}
						else
							if (f18->cf1[0]>>11 == 3) // est' Form3 bez Form2
							{
									f18b = (struct form18b *)fs;
								
								printf("est' form3 bez form12 %d \n",outpack0.svch2.nword);
								if (outpack0.svch2_no.form2[0]>>11 == 2) // bil Form2
								{
									f18b = (struct form18b *)fs;

									for(i=0;i<f18->fsn;i++) 
										outpack0.svch2.word[outpack0.svch2.nword+i]=f18b->cf3[i];
									outpack0.svch2_no.nword += f18->fsn; //	
									printf("\nx=%x y=%x\n",f18b->cf3[2],f18b->cf3[3]);
						
								}
							}
						printf("nword=%d no kvi=5 recieve\n",outpack0.svch2_no.nword);					
												
						break;					
/*					case 6 :
						if ((fs->r0==1)&&(fs->r1==0)&&(fs->r2==0)&&(fs->r3==0)) //pervoe reg soobwenie
						{
							if (outpack0.svch2_no.nword==0)  //��ࢠ� ��ப�
								outpack0.svch2_rli.nword = fsn; //
							else outpack0.svch2_rli.nword += fsn-30; 
							memcpy( &outpack0.svch2_no.form3[mode.no_num2*11],(char *)fs+sizeof(struct sac) + 32 , 22); //form6 //44	
						}
						else 
						{
							if (outpack0.svch2_no.nword==0)  //��ࢠ� ��ப�
								outpack0.svch2_rli.nword = fsn; //
							else outpack0.svch2_rli.nword += fsn-20; 
							memcpy( &outpack0.svch2_no.form3[mode.no_num2*11],(char *)fs+sizeof(struct sac) + 22 , 22); //form6 //44   
			            }
					
						for(i=0;i<10;i++) outpack0.svch2_rli.form1[i]=mode.cf1_svch2[i];
					
			            //for (i=0;i<11;i++) printf(" %04x ",outpack0.svch2_no.form3[mode.no_num2*11+i]);printf("\n");
						mode.no_num2++;
						printf("no_num=%d nword=%d tki recieve\n",mode.no_num2,outpack0.svch2_rli.nword);					
						break;
*/
					case 7 : case 8 :

			            freo = (struct form_reo *)fs;
						printf(" fsn_reo= %x \n ",freo->fsn);

						for(i=0;i<10;i++) outpack0.svch2_reo.form1[i]=freo->cf1[i];
						if (outpack0.svch2_no.nword==0) 
							outpack0.svch2_no.nword += 10; 

						for(i=0;i<freo->fsn - 10;i++) 
							outpack0.svch2.word[outpack0.svch2.nword+i]=freo->cf2[i];
						outpack0.svch2_no.nword += freo->fsn-10; //
						printf("nword=%d reo kvi=7 recieve\n",outpack0.svch2_no.nword);					

						mode.no_num2++;
						printf("no_num=%d nword=%d tki kvi=7,8 recieve\n",mode.no_num2,outpack0.svch2_rli.nword);					
						break;
				}
        }
        if( fs->nf == 27 ) {
            outpack0.link = KRK_LINK_OK;

            //if(( stat.link )||(stat.rli)) {
			if( stat.link ) {
               ResetBuffers2();
               HandlerCmdKasrt27();
               SendOutPack2();
               outpack0.cr_com++;
            }
        }
        if( fs->nf == 193 ) {
            /*f199 = (struct form199 *)b;
            memset( f199, 0, sizeof(struct form199) );
            memcpy( f199, fs, sizeof(struct form193) );
            f199->s.ps = 1;
            f199->s.vr = 0;
            f199->s.kvi = 2;
            f199->s.nf = 199;
            f199->s.r0 = ( ( ( count.out2 / 10000 ) % 1000 ) % 100 ) % 10;
            f199->s.r1 = ( ( ( count.out2 / 10000 ) % 1000 ) % 100 ) / 10;
            f199->s.r2 = ( ( count.out2 / 10000 ) % 1000 ) / 100;
            f199->s.r3 = ( count.out2 / 10000 ) / 1000;
            f199->s.v0 = 0;
            f199->s.v1 = 0;
            f199->s.v2 = 0;
            f199->s.v3 = 0;
            f199->s.a0 = fs->p0;
            f199->s.a1 = fs->p1;
            f199->s.a2 = fs->p2;
            f199->s.a3 = fs->p3;
            f199->s.a4 = fs->p4;
            f199->s.a5 = fs->p5;
            f199->s.p0 = fs->a0;
            f199->s.p1 = fs->a1;
            f199->s.p2 = fs->a2;
            f199->s.p3 = fs->a3;
            f199->s.p4 = fs->a4;
            f199->s.p5 = fs->a5;
            SendSVC2( f199, sizeof(struct form199) );
            count.out2++; */
        }
        if( fs->nf == 199 ) {
            f199 = (struct form199 *)fs;

            /*switch(f199->kfs) {
            case 34: case 39: outpack0.link = KRK_MODE_REO;  break;
            default: outpack0.link = KRK_CMD_OK; break;
            }*/
			
			for(i=0;i<10;i++) printf(" %x ",f199->cf1[i]);printf(" *199.1\n");
			for(i=0;i<5;i++)  printf(" %x ",f199->cf2[i]);printf(" *199.2\n");

//            memcpy( mode,cf1_svch2[0] , f199 +24, 20 );
  //          memcpy( mode,cf2_svch2[0] , f199 +44, 10 );

			for(i=0;i<10;i++) mode.cf1_svch2[i]=f199->cf1[i];
			for(i=0;i<5;i++)  mode.cf2_svch2[i]=f199->cf2[i];
			mode.no_num2=0;
			
            if( stat.link ) {
				//ResetBuffers2(); 
				//HandlerCmdKasrt17(); SendOutPack1();
				outpack2.nload = outpack2.nsave = outpack2.blk = 0;
				outpack0.cr_com++;
				mode.scan2=1;	//���祭�� ����ᮢ ���
				stat.link =0;
				printf("mode.scan2=1\n");
                outpack0.link = KRK_CMD_OK;
            }
            if( stat.rli ) {
                ResetBuffers2();
				outpack2.nload = outpack2.nsave = outpack2.blk = 0;
				outpack0.cr_com++;				
				mode.rli2=1; //���祭�� ����ᮢ ���
				stat.rli=0;
				printf("mode.rli2=1\n");
				outpack0.link = KRK_MODE_REO;
				paket_rli=0;
            }

         }
         if( fs->nf == 203 ) {
            f199 = (struct form199 *)b;
            memset( f199, 0, sizeof(struct form199) );
            /*f199->s.ps = 1;
            f199->s.vr = 0;
            f199->s.kvi = 2;
            f199->s.nf = 199;
            f199->s.r0 = ( ( ( count.out2 / 10000 ) % 1000 ) % 100 ) % 10;
            f199->s.r1 = ( ( ( count.out2 / 10000 ) % 1000 ) % 100 ) / 10;
            f199->s.r2 = ( ( count.out2 / 10000 ) % 1000 ) / 100;
            f199->s.r3 = ( count.out2 / 10000 ) / 1000;
            f199->s.v0 = 0;
            f199->s.v1 = 0;
            f199->s.v2 = 0;
            f199->s.v3 = 0;
            f199->s.a0 = fs->p0;
            f199->s.a1 = fs->p1;
            f199->s.a2 = fs->p2;
            f199->s.a3 = fs->p3;
            f199->s.a4 = fs->p4;
            f199->s.a5 = fs->p5;
            f199->s.p0 = fs->a0;
            f199->s.p1 = fs->a1;
            f199->s.p2 = fs->a2;
            f199->s.p3 = fs->a3;
            f199->s.p4 = fs->a4;
            f199->s.p5 = fs->a5;
            f199->t1 = 0x00;
            f199->t2 = 0x1d;
            f199->kfs = 34;
            SendSVC1( f199, sizeof(struct form199) );
            count.out2++;*/
         }
         break;
      }
   }

   if( sr->svc1 || sr->svc2 || sr->dmv || sr->usoi ) {
      sr->rts = 1;
   } else {
      sr->rts = 0;
   }

   return( 0 );
}

int SendSVC2( const void *buf, unsigned len )
{
   int i;
   struct header12 *h12;
   struct form11 *f11;

   if( verbose > 0 ) {
      printf( "SendSVC2: %d bytes.\n", len );
   }

//---------- Step-1 ----------

	kzo13_2();

   i = outpack2.nsave;
   h12 = (struct header12 *)outpack2.buf[i].data;
   SetHeader12( h12 );
   h12->npol = 1;
   h12->nspol = 1;  
   h12->kss = len / 2;
//   h12->kss = ( len / 2 ) & 0xf; //Temp!!!
 //  h12->kss2 = ( len / 2 ) >> 4; //Temp!!!
   h12->kvi = 2;
   h12->ps = 1;
   h12->kzo = 5;
   memcpy( outpack2.buf[i].data + sizeof(struct header12), buf, len );
   outpack2.buf[i].size = sizeof(struct header12) + len;
   outpack2.buf[i].cmd = BUF3KIT_CMD_BLK2;
   outpack2.nsave++;

	kzo7_2();
//---------- Step 2 ----------

	kzo13_2();	

   i = outpack2.nsave;
   h12 = (struct header12 *)outpack2.buf[i].data;
   SetHeader12( h12 );
   h12->npol = 1;
   h12->nspol = 1;  
   h12->kss = sizeof(struct form11) / 2;
   h12->kvi = 1;
   h12->ps = 1;
   h12->kzo = 5;
   f11 = (struct form11 *)(outpack2.buf[i].data + sizeof(struct header12));
   memcpy( (char *)f11, (char *)&form11k1, sizeof(struct form11) );
   f11->ku0 = 0; //RAB
   f11->ku1 = 1; //PRD-ON
   f11->ku2 = 0; //PRM-OFF
   f11->ku3 = 0; //TKI
   f11->ku4 = 1; //FM1
//      f11->ku5 = 1; //PRD-M1
//      f11->ku6 = 7; //PRM-M7
   f11->ku7 = 0; //PRD-0dB
   f11->ku8 = 0; //FK-0
   f11->ku9z0 = 1; //ZAPROS-0
   f11->ku9z1 = 1; //ZAPROS-1
   f11->ku9z2 = 1; //ZAPROS-2
   f11->ku9z3 = 1; //ZAPROS-3
   f11->ku9z4 = 1; //ZAPROS-4
   f11->ku9z5 = 1; //ZAPROS-5
   f11->ku9z6 = 1; //ZAPROS-6
   f11->ku9z7 = 1; //ZAPROS-7
   f11->ku9z8 = 1; //ZAPROS-8
   f11->ku9z9 = 1; //ZAPROS-9
   f11->ku9z10 = 1; //ZAPROS-10
   memcpy( (char *)&form11k1, (char *)f11, sizeof(struct form11) );
   outpack2.buf[i].size = sizeof(struct header12) + sizeof(struct form11);
   outpack2.buf[i].cmd = BUF3KIT_CMD_BLK2;
   outpack2.nsave++;

	kzo7_2();

	BLKT(1);	BLKT(1);

//---------- Step 3 ----------

	kzo13_2();

   i = outpack2.nsave;
   h12 = (struct header12 *)outpack2.buf[i].data;
   SetHeader12( h12 );
   h12->npol = 1;
   h12->nspol = 1;  
   h12->kss = sizeof(struct form11) / 2;
   h12->kvi = 1;
   h12->ps = 1;
   h12->kzo = 5;
   f11 = (struct form11 *)(outpack2.buf[i].data + sizeof(struct header12));
   memcpy( (char *)f11, (char *)&form11k1, sizeof(struct form11) );
   f11->ku0 = 0; //RAB
   f11->ku1 = 0; //PRD-OFF
   f11->ku2 = 1; //PRM-ON
   f11->ku3 = 0; //TKI
   f11->ku4 = 1; //FM1
   f11->ku7 = 0; //PRD-0dB
   f11->ku8 = 0; //FK-0
   f11->ku9z0 = 1; //ZAPROS-0
   f11->ku9z1 = 1; //ZAPROS-1
   f11->ku9z2 = 1; //ZAPROS-2
   f11->ku9z3 = 1; //ZAPROS-3
   f11->ku9z4 = 1; //ZAPROS-4
   f11->ku9z5 = 1; //ZAPROS-5
   f11->ku9z6 = 1; //ZAPROS-6
   f11->ku9z7 = 1; //ZAPROS-7
   f11->ku9z8 = 1; //ZAPROS-8
   f11->ku9z9 = 1; //ZAPROS-9
   f11->ku9z10 = 1; //ZAPROS-10
   memcpy( (char *)&form11k1, (char *)f11, sizeof(struct form11) );
   outpack2.buf[i].size = sizeof(struct header12) + sizeof(struct form11);
   outpack2.buf[i].cmd = BUF3KIT_CMD_BLK2;
   outpack2.nsave++;

	kzo7_2();	

   SendOutPack2();

   return( 0 );
}

int SendOutPack2( void )
{
   int i,i1;
   int j;
   struct header12 *h12; //!

   if( outpack2.nload >= outpack2.nsave ) {
      return( 0 );
   } else if( ! outpack2.blk ) {
      i = outpack2.nload;
      j = outbuf2.save;
      if( sizeof(outbuf2.data) - j < outpack2.buf[i].size ) {
         return( -1 );
      }
      memcpy( &outbuf2.data[j], outpack2.buf[i].data, outpack2.buf[i].size );
      outbuf2.save += outpack2.buf[i].size;
      outpack2.nload++;
      if( verbose > 1 ) {
			h12 = (struct header12 *)outpack2.buf[i].data;
			//SetHeader12( h12 );
	        printf( "SendOutPack2: size=%d cmd=%08x kvi=%d kzo=%d.\n", 
            outpack2.buf[i].size, outpack2.buf[i].cmd, h12->kvi, h12->kzo );
			for(i1=10;i1<90;i1++) printf("%02x ",outpack2.buf[i].data[i1+j]);
			printf("\n");
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_BLK0 ) {
         outpack2.blk |= BUF3KIT_BLK0;
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_BLK1 ) {
         outpack2.blk |= BUF3KIT_BLK1;
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_BLK2 ) {
         outpack2.blk |= BUF3KIT_BLK2;
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_BLK3 ) {
         outpack2.blk |= BUF3KIT_BLK3;
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_BLK4 ) {
         outpack2.blk |= BUF3KIT_BLK4;
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_BLK5 ) {
         outpack2.blk |= BUF3KIT_BLK5;
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_BLK6 ) {
         outpack2.blk |= BUF3KIT_BLK6;
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_BLKT ) {
         outpack2.blk |= BUF3KIT_BLKT;
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_OUT0 ) {
         SendOutPack0();
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_OUT1 ) {
         SendOutPack1();
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_OUT2 ) {
         SendOutPack2();
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_OUT3 ) {
         SendOutPack3();
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_OUT4 ) {
         SendOutPack4();
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_OUT5 ) {
         SendOutPack5();
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_OUT6 ) {
         SendOutPack6();
      }
 
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_DEC ) {
         outpack2.nload -= outpack2.buf[i].param;
      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_KRK ) {
//         outpack0.krk = outpack2.buf[i].param;
         outpack0.link = outpack2.buf[i].param;

      }
      if( outpack2.buf[i].cmd & BUF3KIT_CMD_END ) {
         outpack2.nsave = outpack2.nload = 0;
         stat.in |= FLAG_BUF2;
         CheckStatus();
         ControlLed2( 0 );
      }
      return( outpack2.buf[i].size ); 
   }

   return( 0 );
}

