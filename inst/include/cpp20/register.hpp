#ifndef CPP20_REGISTER_HPP
#define CPP20_REGISTER_HPP

// The R parser will search for the string "[[cpp20::register]]"
#ifdef __R_GENERATE_
  #define CPP20_REGISTER [[cpp20::register]]
#else
  #define CPP20_REGISTER 
#endif
  

#endif
