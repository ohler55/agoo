class Simple
  def self.call(req)
    [ 200, { }, [ "Simple Simon" ] ]
  end
end

# Use this with the agoo applicaiton in bin/agoo. Make sure the class path is
# set up with a -I and then require the class with a -r.

# agoo -I example -r simple /simple@Simple
