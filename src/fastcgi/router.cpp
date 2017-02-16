#include "router.hpp"


router::router()
{
    FCGX_Init();
    FCGX_InitRequest( &m_request, 0, 0 );
}
void router::run()
{
    for(;;)
    {
        int rc = FCGX_Accept_r( &m_request );
        
        if (rc < 0)
            continue;

        FCGX_FPrintF( m_request.out,
            "Content-Type: application/xml; charset=UTF-8\r\n"
            "Content-Encoding: gzip\r\n"
            "\r\n"
        );
        std::string uri; 
        uri = FCGX_GetParam("REQUEST_URI", m_request.envp);
        std::cout<<uri<<std::endl;
        // int num_bytes_written = FCGX_PutStr( m_test.c_str(), m_test.length(), m_request.out );
        // if( num_bytes_written != m_test.length() || num_bytes_written == -1 )
        // {
        //     break;
        // }
        // boost::thread_group m_thread_group;
        // for (size_t i = 0; i < 1; ++i)
        // {
        //     m_thread_group.create_thread(boost::bind(&redis_api::run,&r));
        //     m_thread_group.create_thread(boost::bind(&pdf_api::run,&p));
        // }

        // m_thread_group.join_all();
        // FCGX_Finish_r( &m_request );
        if(uri=="/redis/")
        {
            redis_api r;
            thread r_thread([&r =m_request]()
            {
                r.run(m_request);
            });
            r_thread.join();
        }
        else if(uri=="/pdf/")
        {
            pdf_api p;
            thread p_thread([&p =m_request]()
            {
                p.run(m_request);
            });
            p_thread.join();
        }
    }
}