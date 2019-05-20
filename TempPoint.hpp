 
#include <vector>
 using std::vector;

 struct TempPoint{
  uint32_t t;
  float val;

  static TempPoint fromString(const String & s,bool useComma = false){
    TempPoint res{0,-1};
    int split = s.indexOf(useComma?',':':');
    if(split>0 && split< s.length()){
      res.t = s.substring(0,split).toInt();
      res.val = s.substring(split+1).toFloat();
    }
    return res;
  }
  
  String toString(bool useComma = false){
    String res((unsigned long)t,10);
    res+=useComma?" , ":" : ";
    res+=String(val);
    return res;
  }
  bool isValid() const{
    return val!=-1;
  }
  bool operator < (const TempPoint & other)const {
    return t<other.t;
  }


  
  void fillData(uint8_t *d) const{
    const uint8_t* tt = reinterpret_cast<const uint8_t*>(&t);
    d[0]=tt[3];
    d[1]=tt[2];
    d[2]=tt[1];
    d[3]=tt[0];
    const uint8_t * v = reinterpret_cast<const uint8_t*>(&val);
    d[4] = v[3];
    d[5] = v[2];
    d[6] = v[1];
    d[7] = v[0];
    //return {t[3],t[2],t[1],t[0],v[3],v[2],v[1],v[0]};
  }

  void appendToFile(File & f){
    auto str =toString();
    f.println(str);
  }

  static bool fillValuesFromFile(File & f,std::vector<TempPoint> & v){
    v.clear();

    if(f.available()){
      Serial.println("restore from saved");
      auto line = f.readStringUntil('\n');
      int i = 1;
      while (line.length() && i < MAX_REC_SIZE)
      {

        auto p = TempPoint::fromString(line);
        if(p.isValid()){
          v.push_back(p);
        //Serial.print("valid line : ");
        //Serial.println(line);
        //Serial.printf("%u : %.2f",p.t,p.val);
        }
        else{
          Serial.print("non valid line : ");
          Serial.println(line);
        }
        line = f.readStringUntil('\n');
        i++;
      }
      f.close();
      return true;
    }
    else{
      f.close();
      return false;
    }


  }

};
